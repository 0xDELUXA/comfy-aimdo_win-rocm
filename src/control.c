#include "plat.h"

uint64_t vram_capacity;
uint64_t total_vram_usage;

SHARED_EXPORT
uint64_t get_total_vram_usage() {
    return total_vram_usage;
}

SHARED_EXPORT
bool init(int cuda_device_id) {
    CUdevice dev;
    char dev_name[256];

    log_reset_shots();

    if (!CHECK_CU(cuDeviceGet(&dev, cuda_device_id)) ||
        !CHECK_CU(cuDeviceTotalMem(&vram_capacity, dev)) ||
        !plat_init(dev)) {
        return false;
    }

    if (!CHECK_CU(cuDeviceGetName(dev_name, sizeof(dev_name), dev))) {
        sprintf(dev_name, "<unknown>");
    }

    log(INFO, "comfy-aimdo inited for GPU: %s (VRAM: %zu MB)\n",
        dev_name, (size_t)(vram_capacity / (1024 * 1024)));
    return true;
}

SHARED_EXPORT
void cleanup() {
    plat_cleanup();
}
