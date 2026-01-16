#include "plat.h"

#include <windows.h>

typedef struct {
    HANDLE hFile;
    HANDLE hMapping;
    void* base_address;
    uint64_t size;
    bool is_mapped;
    CRITICAL_SECTION lock;
} MMAPReservation;

#undef log
#define log(a, ...) fprintf(stderr, __VA_ARGS__);

#define MAX_RESERVATIONS 64
MMAPReservation g_reservations[MAX_RESERVATIONS];

LONG CALLBACK PageFaultHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    uintptr_t faulting_addr;
    if (ExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
        /* Address 1 in ExceptionInformation is the faulting memory address */
        faulting_addr = (uintptr_t)ExceptionInfo->ExceptionRecord->ExceptionInformation[1];

    for (int i = 0; i < MAX_RESERVATIONS; i++) {
        MMAPReservation* res = &g_reservations[i];
        uintptr_t base = (uintptr_t)res->base_address;

        if (!base || faulting_addr < base || faulting_addr >= base + res->size) {
            continue;
        }

        EnterCriticalSection(&res->lock);
        if (res->is_mapped) {
            LeaveCriticalSection(&res->lock);
            return EXCEPTION_CONTINUE_EXECUTION;
        }


        log(DEBUG, "RBAR/VEH: Fault at %p in RBAR@%p. Mapping file...\n", (void*)faulting_addr, res->base_address);

        res->hMapping = CreateFileMapping(res->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!res->hMapping) {
            log(ERROR, "RBAR/VEH: Failed to create file mapping. OS Error: %lu\n", GetLastError());
            LeaveCriticalSection(&res->lock);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (!MapViewOfFile3(res->hMapping, GetCurrentProcess(), res->base_address, 0, res->size,
                            MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, NULL, 0)) {
            log(ERROR, "RBAR/VEH: MapViewOfFile3 failed. Address likely occupied. OS Error: %lu\n", GetLastError());
            LeaveCriticalSection(&res->lock);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        res->is_mapped = true;
        log(DEBUG, "RBAR/VEH: Fault filled successfully at %p\n", res->base_address);
        LeaveCriticalSection(&res->lock);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool one_time_setup_done;

static inline bool one_time_setup() {
    if (one_time_setup_done) {
        return true;
    }
    log(DEBUG, "RBAR: Registering FaultHandler\n");
    if (!AddVectoredExceptionHandler(1, PageFaultHandler)) {
        log(ERROR, "RBAR: Failed to register Exception Handler!\n");
        return false;
    }
    one_time_setup_done = true;
    return true;
}


SHARED_EXPORT
void *rbar_allocate(char *file_path) {
    int i;
    MMAPReservation *res;

    if (!one_time_setup()) {
        return NULL;
    }

    log(DEBUG, "RBAR: Creating RBAR for %s\n", file_path);
    for (i = 0; i < MAX_RESERVATIONS; i++) {
        res = &g_reservations[i];
        if (!res->base_address) {
            break;
        }
    }
    if (i == MAX_RESERVATIONS) {
        log(ERROR, "RBAR: Maximum reservations (%d) reached\n", MAX_RESERVATIONS);
        return NULL;
    }

    res->hFile = CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (res->hFile == INVALID_HANDLE_VALUE) {
        log(ERROR, "RBAR: Could not open file: %s\n", file_path);
        return NULL;
    }

    LARGE_INTEGER fs;
    GetFileSizeEx(res->hFile, &fs);
    res->size = fs.QuadPart;
    InitializeCriticalSection(&res->lock);

    res->base_address = VirtualAlloc2(NULL, NULL, res->size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                      PAGE_NOACCESS, NULL, 0);
    if (!res->base_address) {
        log(ERROR, "RBAR: VirtualAlloc2 failed to reserve address space. OS Error: %lu\n", GetLastError());
        CloseHandle(res->hFile);
        return NULL;
    }

    res->is_mapped = false;

    log(DEBUG, "RBAR: Reserved %llu bytes at %p\n", res->size, res->base_address);

    return res->base_address;
}

SHARED_EXPORT
void rbar_deallocate(void *base_address) {
    for (int i = 0; i < MAX_RESERVATIONS; i++) {
        MMAPReservation* res = &g_reservations[i];
        if (res->base_address != base_address) {
            continue;
        }

        EnterCriticalSection(&res->lock);
        if (res->is_mapped) {
            UnmapViewOfFile2(GetCurrentProcess(), res->base_address, 0);
            if (res->hMapping) {
                CloseHandle(res->hMapping);
            }
            log(DEBUG, "RBAR: Mapping torn down for %p\n", base_address);
        }

        if (!VirtualFree(res->base_address, 0, MEM_RELEASE)) {
            log(ERROR, "RBAR: VirtualFree failed for %p. Error: %lu\n", base_address, GetLastError());
        }

        if (res->hFile && res->hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(res->hFile);
        }
        LeaveCriticalSection(&res->lock);
        DeleteCriticalSection(&res->lock);

        log(DEBUG, "RBAR: Fully unreserved and closed %p\n", base_address);
        memset(res, 0, sizeof(*res));
        return;
    }

    log(ERROR, "RBAR: Attempted to unreserve unknown pointer %p\n", base_address);
}

SHARED_EXPORT
void rbars_unmap_all() {
    log(DEBUG, "RBAR: Clearing all active mappings to non-committed state...\n");
    for (int i = 0; i < MAX_RESERVATIONS; i++) {
        MMAPReservation* res = &g_reservations[i];
        if (!res->base_address) {
            continue;
        }
        EnterCriticalSection(&res->lock);
        if (!res->is_mapped) {
            LeaveCriticalSection(&res->lock);
            continue;
        }
        if (!UnmapViewOfFile2(GetCurrentProcess(), res->base_address, MEM_PRESERVE_PLACEHOLDER)) {
            log(ERROR, "RBAR: Atomic unmap failed at %p. Error: %lu\n", res->base_address, GetLastError());
            LeaveCriticalSection(&res->lock);
            continue;
        }
        if (res->hMapping) {
            CloseHandle(res->hMapping);
        }
            
        res->is_mapped = false;
        LeaveCriticalSection(&res->lock);
        log(DEBUG, "RBAR: Unmapped %p\n", g_reservations[i].base_address);
    }
}
