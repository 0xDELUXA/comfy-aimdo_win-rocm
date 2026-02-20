#define _GNU_SOURCE
#include "plat.h"
#include <dlfcn.h>

bool aimdo_setup_hooks() {
    true_cuda_malloc_async = dlsym(RTLD_NEXT, "cudaMallocAsync");
    true_cuda_free_async = dlsym(RTLD_NEXT, "cudaFreeAsync");

    if (!true_cuda_malloc_async || !true_cuda_free_async) {
        log(ERROR, "%s: Failed to find real cudaAsync symbols: %s", __func__, dlerror());
        return false;
    }

    log(DEBUG, "%s: Linux async symbols resolved", __func__);
    return true;
}

void aimdo_teardown_hooks() {}
