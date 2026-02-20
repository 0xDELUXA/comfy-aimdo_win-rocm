#define _GNU_SOURCE
#include "plat.h"
#include <dlfcn.h>
#include <link.h>

static int dump_libs_callback(struct dl_phdr_info *info, size_t size, void *data) {
    /* The first entry is usually the main executable (empty name) */
    const char *lib_name = (info->dlpi_name && info->dlpi_name[0] != '\0') ? info->dlpi_name
                                                                           : "<main_executable>";

    log(DEBUG, "  Mapped Lib: %s (at %p)\n", lib_name, (void *)info->dlpi_addr);
    return 0;
}

static void dump_all_process_libs() {
    log(DEBUG, "--- Beginning Process Library Dump ---\n");
    dl_iterate_phdr(dump_libs_callback, NULL);
    log(DEBUG, "--- End of Library Dump ---\n");
}

bool aimdo_setup_hooks() {
    true_cuda_malloc_async = dlsym(RTLD_NEXT, "cudaMallocAsync");
    true_cuda_free_async = dlsym(RTLD_NEXT, "cudaFreeAsync");

    if (!true_cuda_malloc_async || !true_cuda_free_async) {
        log(ERROR, "%s: Failed to find real cudaAsync symbols: %s\n", __func__, dlerror());
        dump_all_process_libs();
        return false;
    }

    log(DEBUG, "%s: Linux async symbols resolved\n", __func__);
    return true;
}

void aimdo_teardown_hooks() {}
