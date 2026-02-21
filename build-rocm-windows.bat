@echo off
setlocal enabledelayedexpansion

REM Build script for comfy-aimdo on Windows ROCm
REM
REM Requirements:
REM   - ROCm SDK installed via pip (_rocm_sdk_core)
REM   - CUDA toolkit installed (used only as a hipify reference, not for compilation)
REM   - Visual Studio 2022 with C++ workload

echo ============================================
echo   Building comfy-aimdo for Windows ROCm
echo ============================================
echo.

REM ---- Prompt for paths ----
echo Enter the path to your ROCm SDK core directory.
echo This is typically inside your venv, e.g.:
echo   C:\ComfyUI\venv\Lib\site-packages\_rocm_sdk_core
echo.
set /p ROCM_PATH="ROCm SDK path: "

echo.
echo Enter the path to your CUDA toolkit installation, e.g.:
echo   C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9
echo.
set /p CUDA_ORIG="CUDA path: "

echo.

REM ---- Validate inputs ----
if not exist "%ROCM_PATH%" (
    echo ERROR: ROCm path not found: %ROCM_PATH%
    exit /b 1
)
if not exist "%CUDA_ORIG%" (
    echo ERROR: CUDA path not found: %CUDA_ORIG%
    exit /b 1
)

REM ---- Derive paths ----
set ROCM_CLANG=%ROCM_PATH%\lib\llvm\lib\clang
set CLANG_EXE=%ROCM_PATH%\lib\llvm\bin\clang.exe
set LLDLINK_EXE=%ROCM_PATH%\lib\llvm\bin\lld-link.exe
set HIP_INCLUDE=%ROCM_PATH%\include
set HIP_LIB=%ROCM_PATH%\lib

for /d %%i in ("%ROCM_CLANG%\*") do set CLANG_RESOURCE_DIR=%%i

if not defined CLANG_RESOURCE_DIR (
    echo ERROR: Could not find clang version directory under %ROCM_CLANG%
    exit /b 1
)
if not exist "%CLANG_EXE%" (
    echo ERROR: clang.exe not found at %CLANG_EXE%
    exit /b 1
)

echo ROCm path:  %ROCM_PATH%
echo CUDA path:  %CUDA_ORIG%
echo Clang:      %CLANG_EXE%
echo Clang dir:  %CLANG_RESOURCE_DIR%
echo.

REM ---- CUDA junction (hipify needs a path without spaces) ----
set CUDA_LINK=C:\cuda_temp
if exist "%CUDA_LINK%" rmdir "%CUDA_LINK%"
mklink /J "%CUDA_LINK%" "%CUDA_ORIG%" >nul 2>&1

REM ---- Copy source files ----
if exist hip_src rmdir /S /Q hip_src
mkdir hip_src
mkdir hip_src\win
copy src\*.c hip_src\ >nul
copy src\*.h hip_src\ >nul
copy src\win\*.c hip_src\win\ >nul
copy src\win\*.h hip_src\win\ >nul

if exist plat-windows-rocm.c (
    echo Using existing plat-windows-rocm.c...
    copy plat-windows-rocm.c hip_src\plat-windows-rocm.c >nul
)

REM ---- HIPify ----
echo Converting CUDA to HIP...
for %%f in (hip_src\*.h hip_src\*.c hip_src\win\*.h) do (
    hipify-clang --default-preprocessor --clang-resource-directory="%CLANG_RESOURCE_DIR%" --cuda-path=%CUDA_LINK% --cuda-gpu-arch=sm_52 --inplace "%%f" 2>nul
)

REM ---- Manual type replacements (hipify misses some) ----
echo Applying type replacements...
powershell -Command "$files = @(Get-ChildItem hip_src\*.h | ForEach-Object { $_.FullName }) + @(Get-ChildItem hip_src\*.c | ForEach-Object { $_.FullName }) + @(Get-ChildItem hip_src\win\*.c | ForEach-Object { $_.FullName }); foreach ($file in $files) { if (-not (Test-Path $file)) { continue }; $content = Get-Content $file -Raw; $content = $content -replace '#include <cuda\.h>', '#include <hip/hip_runtime.h>'; $content = $content -replace '#include <cuda_runtime\.h>', '#include <hip/hip_runtime.h>'; $content = $content -replace '\bCUdevice\b', 'hipDevice_t'; $content = $content -replace '\bCUdeviceptr\b', 'hipDeviceptr_t'; $content = $content -replace '\bCUresult\b', 'hipError_t'; $content = $content -replace '\bCUDA_SUCCESS\b', 'hipSuccess'; $content = $content -replace '\bCUDA_ERROR_OUT_OF_MEMORY\b', 'hipErrorOutOfMemory'; $content = $content -replace '\bCUmemGenericAllocationHandle\b', 'hipMemGenericAllocationHandle_t'; $content = $content -replace '\bcudaStream_t\b', 'hipStream_t'; $content = $content -replace '\bcuMemGetInfo\b', 'hipMemGetInfo'; $content = $content -replace '\bcuDeviceGet\b', 'hipDeviceGet'; $content = $content -replace '\bcuDeviceTotalMem\b', 'hipDeviceTotalMem'; $content = $content -replace '\bcuDeviceGetName\b', 'hipDeviceGetName'; $content = $content -replace '\bcuMemUnmap\b', 'hipMemUnmap'; $content = $content -replace '\bcuMemRelease\b', 'hipMemRelease'; $content = $content -replace '\bcuMemAddressReserve\b', 'hipMemAddressReserve'; $content = $content -replace '\bcuMemAddressFree\b', 'hipMemAddressFree'; $content = $content -replace '\bcuMemCreate\b', 'hipMemCreate'; $content = $content -replace '\bcuMemSetAccess\b', 'hipMemSetAccess'; $content = $content -replace '\bcuMemMap\b', 'hipMemMap'; $content = $content -replace '\bcuCtxSynchronize\b', 'hipDeviceSynchronize'; Set-Content $file $content }" >nul

REM ---- Generate AMD DXGI-based cuDeviceGetLuid implementation ----
echo Generating ROCm platform stubs...
if exist hip_src\rocm_stubs.c del /F /Q hip_src\rocm_stubs.c
(
echo #include ^<hip/hip_runtime.h^>
echo #include ^<windows.h^>
echo #include ^<dxgi1_4.h^>
echo #include ^<string.h^>
echo.
echo hipError_t cuDeviceGetLuid^(char *luid, unsigned int *deviceNodeMask, hipDevice_t dev^) {
echo     HRESULT hr;
echo     IDXGIFactory1 *factory = NULL;
echo     IDXGIAdapter1 *adapter = NULL;
echo     DXGI_ADAPTER_DESC1 desc;
echo.
echo     hr = CreateDXGIFactory1^(^&IID_IDXGIFactory1, ^(void**^)^&factory^);
echo     if ^(FAILED^(hr^)^) return hipErrorNotSupported;
echo.
echo     UINT adapterIndex = 0;
echo     while ^(factory-^>lpVtbl-^>EnumAdapters1^(factory, adapterIndex, ^&adapter^) == S_OK^) {
echo         hr = adapter-^>lpVtbl-^>GetDesc1^(adapter, ^&desc^);
echo         if ^(SUCCEEDED^(hr^) ^&^& desc.VendorId == 0x1002^) {
echo             if ^(luid^) memcpy^(luid, ^&desc.AdapterLuid, sizeof^(LUID^)^);
echo             if ^(deviceNodeMask^) *deviceNodeMask = 1;
echo             adapter-^>lpVtbl-^>Release^(adapter^);
echo             factory-^>lpVtbl-^>Release^(factory^);
echo             return hipSuccess;
echo         }
echo         if ^(adapter^) { adapter-^>lpVtbl-^>Release^(adapter^); adapter = NULL; }
echo         adapterIndex++;
echo     }
echo     if ^(factory^) factory-^>lpVtbl-^>Release^(factory^);
echo     return hipErrorNotSupported;
echo }
) > hip_src\rocm_stubs.c

powershell -Command "$file = 'hip_src\plat.h'; $content = Get-Content $file -Raw; if ($content -notmatch 'hipError_t cuDeviceGetLuid') { $content = $content + \"`r`n`r`nhipError_t cuDeviceGetLuid(char *luid, unsigned int *deviceNodeMask, hipDevice_t dev);`r`n\"; Set-Content $file $content -NoNewline }" >nul

REM ---- Compile each source to an object file ----
echo.
echo Compiling...
set HIP_SRC_PATH=%CD%\hip_src
if not exist comfy_aimdo mkdir comfy_aimdo
if not exist obj mkdir obj

REM ---- Visual Studio Path Prompt ----
echo Enter the path to your Visual Studio installation directory.
echo This is the folder containing the 'VC' folder, e.g.:
echo   C:\Program Files\Microsoft Visual Studio\2022\Community
echo.
set /p VS_PATH="Visual Studio Path: "

if not exist "%VS_PATH%\VC\Tools\MSVC\" (
    echo ERROR: Visual Studio path invalid or MSVC tools missing: %VS_PATH%
    exit /b 1
)

echo Found VS at: %VS_PATH% 

REM ---- Auto-detect MSVC Version ----
set MSVC_VER=
for /f "delims=" %%i in ('dir /b /ad /on "%VS_PATH%\VC\Tools\MSVC\"') do set MSVC_VER=%%i
set "MSVC_VER_PATH=%VS_PATH%\VC\Tools\MSVC\%MSVC_VER%" 

REM ---- Auto-detect Windows SDK (Checking D: then C:) ----
set "WINSDK_BASE="
if exist "D:\Program Files (x86)\Windows Kits\10" set "WINSDK_BASE=D:\Program Files (x86)\Windows Kits\10"
if not defined WINSDK_BASE if exist "C:\Program Files (x86)\Windows Kits\10" set "WINSDK_BASE=C:\Program Files (x86)\Windows Kits\10"
if not defined WINSDK_BASE if exist "D:\Program Files\Windows Kits\10" set "WINSDK_BASE=D:\Program Files\Windows Kits\10"
if not defined WINSDK_BASE if exist "C:\Program Files\Windows Kits\10" set "WINSDK_BASE=C:\Program Files\Windows Kits\10"

if not defined WINSDK_BASE (
    echo ERROR: Windows SDK 10 not found.
    exit /b 1
)

set WINSDK_VER=
for /f "delims=" %%i in ('dir /b /ad /on "%WINSDK_BASE%\Include\"') do set WINSDK_VER=%%i
set "WINSDK_INC_PATH=%WINSDK_BASE%\Include\%WINSDK_VER%" [cite: 29]
set "WINSDK_LIB_PATH=%WINSDK_BASE%\Lib\%WINSDK_VER%" [cite: 29]

echo Found WinSDK at: %WINSDK_BASE% (%WINSDK_VER%) [cite: 29]

set "COMPILE_FLAGS=-D__HIP_PLATFORM_AMD__ -O3 -fms-extensions -fms-compatibility -I"%HIP_SRC_PATH%" -I"%HIP_INCLUDE%" -isystem "%CLANG_RESOURCE_DIR%\include" -isystem "%MSVC_VER_PATH%\include" -isystem "%WINSDK_INC_PATH%\ucrt" -isystem "%WINSDK_INC_PATH%\shared" -isystem "%WINSDK_INC_PATH%\um""

set OBJ_FILES=
for %%f in (hip_src\*.c hip_src\win\*.c) do (
    echo Compiling %%f...
    "%CLANG_EXE%" %COMPILE_FLAGS% -c "%%f" -o "obj\%%~nf.obj"
    if errorlevel 1 (
        echo COMPILE ERROR in %%f
        goto :buildfailed
    )
    set "OBJ_FILES=!OBJ_FILES! "obj\%%~nf.obj""
)

REM ---- Linking ----
echo.
echo Linking...
set "MSVC_LIB=%MSVC_VER_PATH%\lib\x64"

if exist comfy_aimdo\aimdo.dll del comfy_aimdo\aimdo.dll

"%LLDLINK_EXE%" ^
    -out:comfy_aimdo\aimdo.dll ^
    -dll ^
    -nologo ^
    "-libpath:%HIP_LIB%" ^
    "-libpath:%MSVC_LIB%" ^
    "-libpath:%WINSDK_LIB_PATH%\ucrt\x64" ^
    "-libpath:%WINSDK_LIB_PATH%\um\x64" ^
    "-libpath:%CLANG_RESOURCE_DIR%\lib\windows" ^
    "%CLANG_RESOURCE_DIR%\lib\windows\clang_rt.builtins-x86_64.lib" ^
    amdhip64.lib ^
    dxgi.lib ^
    dxguid.lib ^
    libcmt.lib ^
    %OBJ_FILES%

set BUILD_RESULT=%ERRORLEVEL%
goto :cleanup

:buildfailed
set BUILD_RESULT=1

:cleanup
if exist "%CUDA_LINK%" rmdir "%CUDA_LINK%"
if exist obj rmdir /S /Q obj

echo.
if %BUILD_RESULT% EQU 0 (
    if exist comfy_aimdo\aimdo.dll (
        echo ============================================
        echo          BUILD SUCCESSFUL
        echo ============================================
        echo Output: comfy_aimdo\aimdo.dll
        for %%F in (comfy_aimdo\aimdo.dll) do echo Size:   %%~zF bytes
        echo.
        echo Now run: pip install .
		echo.
		echo After that, one manual step:
        for /f "tokens=*" %%i in ('python -c "import site; print(site.getsitepackages()[0])"') do set "VENV_SITE=%%i"
        echo Copy %ROCM_PATH%\bin\amdhip64_7.dll
        echo   to !VENV_SITE!\comfy_aimdo\
        echo   Without this it may use the system-wide version or fail to
        echo   load entirely because the dependency cannot be resolved
        echo   from within the virtual environment.
        echo ============================================
    ) else (
        echo ============================================
        echo BUILD FAILED - DLL not produced
        echo ============================================
        exit /b 1
    )
) else (
    echo ============================================
    echo BUILD FAILED - error code %BUILD_RESULT%
    echo ============================================
    exit /b %BUILD_RESULT%
)