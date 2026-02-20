#include "plat.h"
#include <windows.h>
#include <detours.h>

#define TARGET_DLL "cudart64_12.dll"

int (WINAPI *true_cuda_malloc)(void**, size_t) = NULL;
int (WINAPI *true_cuda_free)(void*) = NULL;

bool aimdo_setup_hooks() {
    HMODULE h_real_cuda;
    int status;

    h_real_cuda = GetModuleHandleA(TARGET_DLL);
    if (h_real_cuda == NULL) {
        h_real_cuda = LoadLibraryExA(TARGET_DLL, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    if (h_real_cuda == NULL) {
        log(ERROR, "%s: %s not found", __func__, TARGET_DLL);
        return false;
    }

    true_cuda_malloc = (int (WINAPI *)(void**, size_t))GetProcAddress(h_real_cuda, "cudaMalloc");
    true_cuda_free = (int (WINAPI *)(void*))GetProcAddress(h_real_cuda, "cudaFree");

    if (true_cuda_malloc == NULL || true_cuda_free == NULL) {
        log(ERROR, "%s: Failed to resolve exports. Malloc: %p, Free: %p",
            __func__, (void*)true_cuda_malloc, (void*)true_cuda_free);
        return false;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (DetourAttach((PVOID*)&true_cuda_malloc, aimdo_cuda_malloc) != 0) {
        log(ERROR, "%s: DetourAttach failed for cudaMalloc", __func__);
        DetourTransactionAbort();
        return false;
    }

    if (DetourAttach((PVOID*)&true_cuda_free, aimdo_cuda_free) != 0) {
        log(ERROR, "%s: DetourAttach failed for cudaFree", __func__);
        DetourTransactionAbort();
        return false;
    }

    status = (int)DetourTransactionCommit();
    if (status != 0) {
        log(ERROR, "%s: DetourTransactionCommit failed: %d", __func__, status);
        return false;
    }

    log(DEBUG, "%s: hooks successfully installed", __func__);
    return true;
}

void aimdo_teardown_hooks() {
    int status;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach((PVOID*)&true_cuda_malloc, aimdo_cuda_malloc);
    DetourDetach((PVOID*)&true_cuda_free, aimdo_cuda_free);

    status = (int)DetourTransactionCommit();
    if (status != 0) {
        log(ERROR, "%s: DetourDetach failed: %d", __func__, status);
    } else {
        log(DEBUG, "%s: hooks successfully removed", __func__);
    }
}
