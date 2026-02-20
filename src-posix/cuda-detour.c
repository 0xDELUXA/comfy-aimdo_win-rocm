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

static int find_cudart_callback(struct dl_phdr_info *info, size_t size, void *data) {
    if (strstr(info->dlpi_name, "libcudart")) {
        void* handle = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
        if (handle) {
            true_cuda_malloc_async = dlsym(handle, "cudaMallocAsync");
            true_cuda_free_async = dlsym(handle, "cudaFreeAsync");
            dlclose(handle);

            if (true_cuda_malloc_async && true_cuda_free_async) {
                log(DEBUG, "aimdo: Cudart symbols resolved from %s\n", info->dlpi_name);
                return 1;
            }
        }
    }
    return 0;
}

bool aimdo_setup_hooks() {
    dl_iterate_phdr(find_cudart_callback, NULL);

    if (!true_cuda_malloc_async || !true_cuda_free_async) {
        log(ERROR, "%s: Failed to locate libcudart symbols in process memory.\n", __func__);
        dump_all_process_libs();
        return false;
    }

    return true;
}

void aimdo_teardown_hooks() {}
