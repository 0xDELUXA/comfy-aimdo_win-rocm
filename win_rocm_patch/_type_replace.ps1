# _type_replace.ps1
# Applies CUDA -> HIP type/function renames that hipify-clang misses.
# Run from the project root (where hip_src/ lives).

$files = @(Get-ChildItem hip_src\*.h) + @(Get-ChildItem hip_src\*.c)

foreach ($file in $files) {
    $f = $file.FullName
    if (-not (Test-Path $f)) { continue }
    $c = Get-Content $f -Raw

    $c = $c -replace '#include <cuda\.h>',                       '#include <hip/hip_runtime.h>'
    $c = $c -replace '#include <cuda_runtime\.h>',               '#include <hip/hip_runtime.h>'
    $c = $c -replace '\bCUdevice\b',                             'hipDevice_t'
    $c = $c -replace '\bCUdeviceptr\b',                          'hipDeviceptr_t'
    $c = $c -replace '\bCUresult\b',                             'hipError_t'
    $c = $c -replace '\bCUDA_SUCCESS\b',                         'hipSuccess'
    $c = $c -replace '\bCUDA_ERROR_OUT_OF_MEMORY\b',             'hipErrorOutOfMemory'
    $c = $c -replace '\bCUmemGenericAllocationHandle\b',         'hipMemGenericAllocationHandle_t'
    $c = $c -replace '\bcudaStream_t\b',                         'hipStream_t'
    $c = $c -replace '\bCUstream\b',                             'hipStream_t'
    $c = $c -replace '\bCUmemAllocationProp\b',                  'hipMemAllocationProp'
    $c = $c -replace '\bCUmemAccessDesc\b',                      'hipMemAccessDesc'
    $c = $c -replace '\bCU_MEM_ALLOCATION_TYPE_PINNED\b',        'hipMemAllocationTypePinned'
    $c = $c -replace '\bCU_MEM_LOCATION_TYPE_DEVICE\b',          'hipMemLocationTypeDevice'
    $c = $c -replace '\bCU_MEM_ACCESS_FLAGS_PROT_READWRITE\b',   'hipMemAccessFlagsProtReadWrite'
    $c = $c -replace '\bcuMemGetInfo\b',                         'hipMemGetInfo'
    $c = $c -replace '\bcuDeviceGet\b',                          'hipDeviceGet'
    $c = $c -replace '\bcuDeviceTotalMem\b',                     'hipDeviceTotalMem'
    $c = $c -replace '\bcuDeviceGetName\b',                      'hipDeviceGetName'
    $c = $c -replace '\bcuMemUnmap\b',                           'hipMemUnmap'
    $c = $c -replace '\bcuMemRelease\b',                         'hipMemRelease'
    $c = $c -replace '\bcuMemAddressReserve\b',                  'hipMemAddressReserve'
    $c = $c -replace '\bcuMemAddressFree\b',                     'hipMemAddressFree'
    $c = $c -replace '\bcuMemCreate\b',                          'hipMemCreate'
    $c = $c -replace '\bcuMemSetAccess\b',                       'hipMemSetAccess'
    $c = $c -replace '\bcuMemMap\b',                             'hipMemMap'
    $c = $c -replace '\bcuCtxSynchronize\b',                     'hipDeviceSynchronize'
    $c = $c -replace '\bcuCtxGetDevice\b',                       'hipGetDevice'
    $c = $c -replace '\bhipCtxGetDevice\b',                      'hipGetDevice'
    $c = $c -replace '\bhipGetDevice\(&',                        'hipGetDevice((int*)&'
    $c = $c -replace '\bcuMemAllocAsync\b',                      'hipMallocAsync'
    $c = $c -replace '\bcuMemFreeAsync\b',                       'hipFreeAsync'
    $c = $c -replace 'typedef struct CUstream_st \*hipStream_t;\r?\n', ''
    $c = $c -replace '\bcuGetErrorString\s*\(([^,]+),\s*&(\w+)\s*\)', '(($2 = hipGetErrorString($1)) == NULL ? hipErrorUnknown : hipSuccess)'
    $c = $c -replace '\bcuGetErrorString\b',                     'hipGetErrorString'

    if ($f -notlike '*shmem-detect.c' -and $f -notlike '*plat.h') {
        $c = $c -replace '\bwddm_budget_deficit\b',              'cuda_budget_deficit'
    }

    Set-Content $f $c
}

Write-Host "  Type replacements applied."
