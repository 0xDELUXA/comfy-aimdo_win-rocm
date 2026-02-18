#define _GNU_SOURCE  // Must be at the very top
#include "plat.h"
#include <dlfcn.h>
#include <stddef.h>

bool torch_init() {
    // Open the specific library to access local symbols
    void* handle = dlopen("libtorch_python.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) handle = RTLD_DEFAULT;

    // Use the symbol you found in your nm scan
    empty_cache = (void(*)(MempoolId_t))dlsym(handle, "_Z21THCPModule_emptyCacheP7_objectS0_");

    if (!empty_cache) {
        log(ERROR, "torch_init: Could not resolve Linux C++ emptyCache symbol.\n");
    } else {
        log(DEBUG, "torch_init: Hooked Linux C++ emptyCache at %p\n", (void*)empty_cache);
    }

    return true;
}
