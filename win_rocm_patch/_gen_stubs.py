"""
_gen_stubs.py
Generates hip_src/rocm_stubs.c (DXGI-based cuDeviceGetLuid) and
hip_src/hip-detour.c (real Detours hooks for amdhip64_7.dll), then
appends the cuDeviceGetLuid declaration to hip_src/plat.h if missing.
"""
import os

# ---------------------------------------------------------------------------
# rocm_stubs.c
# ---------------------------------------------------------------------------
stubs_c = """\
#include <hip/hip_runtime.h>
#include <windows.h>
#include <dxgi1_4.h>
#include <string.h>

hipError_t cuDeviceGetLuid(char *luid, unsigned int *deviceNodeMask, hipDevice_t dev) {
    HRESULT hr;
    IDXGIFactory1 *factory = NULL;
    IDXGIAdapter1 *adapter = NULL;
    DXGI_ADAPTER_DESC1 desc;

    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory);
    if (FAILED(hr)) return hipErrorNotSupported;

    UINT adapterIndex = 0;
    while (factory->lpVtbl->EnumAdapters1(factory, adapterIndex, &adapter) == S_OK) {
        hr = adapter->lpVtbl->GetDesc1(adapter, &desc);
        if (SUCCEEDED(hr) && desc.VendorId == 0x1002) {
            if (luid) memcpy(luid, &desc.AdapterLuid, sizeof(LUID));
            if (deviceNodeMask) *deviceNodeMask = 1;
            adapter->lpVtbl->Release(adapter);
            factory->lpVtbl->Release(factory);
            return hipSuccess;
        }
        if (adapter) { adapter->lpVtbl->Release(adapter); adapter = NULL; }
        adapterIndex++;
    }
    if (factory) factory->lpVtbl->Release(factory);
    return hipErrorNotSupported;
}
"""

detour_c = """\
#include "plat.h"
#include <windows.h>
#include <detours.h>

#define TARGET_DLL "amdhip64_7.dll"

static int (*true_hip_malloc)(void**, size_t) = NULL;
static int (*true_hip_free)(void*) = NULL;
static int (*true_hip_malloc_async)(void **devPtr, size_t size, void *hStream);
static int (*true_hip_free_async)(void *devPtr, void *hStream);

typedef struct {
    void **true_ptr;
    void *hook_ptr;
    const char *name;
} HookEntry;

static const HookEntry hooks[] = {
    { (void**)&true_hip_malloc,       aimdo_cuda_malloc,       "hipMalloc"       },
    { (void**)&true_hip_free,         aimdo_cuda_free,         "hipFree"         },
    { (void**)&true_hip_malloc_async, aimdo_cuda_malloc_async, "hipMallocAsync"  },
    { (void**)&true_hip_free_async,   aimdo_cuda_free_async,   "hipFreeAsync"    },
};

bool aimdo_setup_hooks(void) {
    HMODULE h_real_hip;
    int status;

    h_real_hip = GetModuleHandleA(TARGET_DLL);
    if (h_real_hip == NULL) {
        h_real_hip = LoadLibraryA(TARGET_DLL);
    }

    if (h_real_hip == NULL) {
        log(ERROR, "%s: %s not found", __func__, TARGET_DLL);
        return false;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    for (int i = 0; i < sizeof(hooks)/sizeof(hooks[0]); i++) {
        *hooks[i].true_ptr = (void*)GetProcAddress(h_real_hip, hooks[i].name);
        if (!*hooks[i].true_ptr ||
            DetourAttach(hooks[i].true_ptr, hooks[i].hook_ptr) != 0) {
            log(ERROR, "%s: Hook %s failed %p", __func__, hooks[i].name, *hooks[i].true_ptr);
            DetourTransactionAbort();
            return false;
        }
    }

    status = (int)DetourTransactionCommit();
    if (status != 0) {
        log(ERROR, "%s: DetourTransactionCommit failed: %d", __func__, status);
        return false;
    }

    return true;
}

void aimdo_teardown_hooks(void) {
    int status;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    for (int i = 0; i < sizeof(hooks) / sizeof(hooks[0]); i++) {
        if (*hooks[i].true_ptr) {
            DetourDetach(hooks[i].true_ptr, hooks[i].hook_ptr);
        }
    }

    status = (int)DetourTransactionCommit();
    if (status != 0) {
        log(ERROR, "%s: DetourDetach failed: %d", __func__, status);
    } else {
        log(DEBUG, "%s: hooks successfully removed", __func__);
    }
}
"""

for out, content in [('hip_src/rocm_stubs.c', stubs_c), ('hip_src/hip-detour.c', detour_c)]:
    if os.path.exists(out):
        os.remove(out)
    with open(out, 'w') as f:
        f.write(content)
    print(f'  Generated: {out}')

# ---------------------------------------------------------------------------
# plat.h -- append declaration if missing
# ---------------------------------------------------------------------------
plat = 'hip_src/plat.h'
try:
    content = open(plat).read()
except FileNotFoundError:
    print(f'  WARNING: {plat} not found, skipping declaration patch.')
else:
    decl = 'hipError_t cuDeviceGetLuid(char *luid, unsigned int *deviceNodeMask, hipDevice_t dev);'
    if decl not in content:
        with open(plat, 'a') as f:
            f.write('\n\n' + decl + '\n')
        print(f'  Appended cuDeviceGetLuid declaration to {plat}')
    else:
        print(f'  {plat} : cuDeviceGetLuid already declared.')
