"""
_gen_stubs.py
Generates hip_src/rocm_stubs.c (DXGI-based cuDeviceGetLuid + no-op Detour stubs)
and appends the cuDeviceGetLuid declaration to hip_src/plat.h if missing.
Run from the project root (where hip_src/ lives).
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

#include <stdbool.h>
#include <stddef.h>

/* cuda-detour.c stubs for ROCm -- no CUDA hook detours needed */
bool aimdo_setup_hooks(void) { return true; }
void aimdo_teardown_hooks(void) {}
"""

out = 'hip_src/rocm_stubs.c'
if os.path.exists(out):
    os.remove(out)
with open(out, 'w') as f:
    f.write(stubs_c)
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
