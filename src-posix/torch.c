#include "plat.h"
#include <dlfcn.h>
#include <stddef.h>

bool torch_init() {
    // 1. RTLD_DEFAULT searches all loaded libraries in the process.
    // Since PyTorch is already running this, we don't need to find the specific .so path.
    void* handle = RTLD_DEFAULT;

    // 2. The Itanium mangled name for c10::cuda::CUDACachingAllocator::emptyCache()
    // signature: void emptyCache(MempoolId_t) where MempoolId_t has a default.
    // Note: On Linux, sometimes the 0-arg version is a distinct symbol.
    empty_cache = (void(*)(void))dlsym(handle, "_ZN3c104cuda21CUDACachingAllocator10emptyCacheENS0_11MempoolId_tE");

    if (!empty_cache) {
        // Fallback for older versions or different overloads
        empty_cache = (void(*)(void))dlsym(handle, "_ZN3c104cuda21CUDACachingAllocator10emptyCacheEv");
    }

    if (!empty_cache) {
        log(ERROR, "torch_init: Could not resolve Linux C++ emptyCache symbol.\n");
    } else {
        log(DEBUG, "torch_init: Hooked Linux C++ emptyCache at %p\n", empty_cache);
    }

    return true;
}
