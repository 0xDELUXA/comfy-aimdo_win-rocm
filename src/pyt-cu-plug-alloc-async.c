#include "plat.h"

int (WINAPI *true_cuda_malloc_async)(void **devPtr, size_t size, void *hStream);
int (WINAPI *true_cuda_free_async)(void *devPtr, void *hStream);

#define CUDA_PAGE_SIZE (2 << 20)
#define ALIGN_UP(s) (((s) + CUDA_PAGE_SIZE - 1) & ~(CUDA_PAGE_SIZE - 1))
#define SIZE_HASH_SIZE 1024

typedef struct SizeEntry {
    void *ptr;
    size_t size;
    struct SizeEntry *next;
} SizeEntry;

static SizeEntry *size_table[SIZE_HASH_SIZE];

static inline unsigned int size_hash(void *ptr) {
    return ((uintptr_t)ptr >> 10 ^ (uintptr_t)ptr >> 21) % SIZE_HASH_SIZE;
}

int WINAPI aimdo_cuda_malloc_async(void **devPtr, size_t size, void *hStream) {
    int device;
    int status;

    CHECK_CU(cuCtxGetDevice(&device));
    vbars_free(wddm_budget_deficit(device, size));

    status = true_cuda_malloc_async(devPtr, size, hStream);

    if (status != 0) {
        vbars_free(size);
        status = true_cuda_malloc_async(devPtr, size, hStream);
    }

    if (status != 0) {
        return status;
    }

    total_vram_usage += ALIGN_UP(size);

    {
        unsigned int h = size_hash(*devPtr);
        SizeEntry *entry = (SizeEntry *)malloc(sizeof(*entry));
        if (entry) {
            entry->ptr = *devPtr;
            entry->size = size;
            entry->next = size_table[h];
            size_table[h] = entry;
        }
    }

    return status;
}

int WINAPI aimdo_cuda_free_async(void *devPtr, void *hStream) {
    SizeEntry *entry;
    SizeEntry **prev;
    unsigned int h;
    int status;

    if (!devPtr) {
        return 0;
    }

    h = size_hash(devPtr);
    entry = size_table[h];
    prev = &size_table[h];

    while (entry) {
        if (entry->ptr == devPtr) {
            *prev = entry->next;

            status = true_cuda_free_async(devPtr, hStream);
            if (status == 0) {
                total_vram_usage -= ALIGN_UP(entry->size);
            }

            free(entry);
            return status;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    log(ERROR, "%s: could not account free at %p\n", devPtr);
    return true_cuda_free_async(devPtr, hStream);
}
