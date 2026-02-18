#include "plat.h"

#include <windows.h>

// Helper to find the mangled name dynamically
static void* find_torch_symbol(HMODULE hModule) {
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    IMAGE_DATA_DIRECTORY exportDataDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hModule + exportDataDir.VirtualAddress);

    DWORD* names = (DWORD*)((BYTE*)hModule + exportDir->AddressOfNames);
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        char* name = (char*)((BYTE*)hModule + names[i]);
        // Matches the core components of the mangled string
        if (strstr(name, "emptyCache") && strstr(name, "CUDACachingAllocator")) {
            log(DEBUG, "%s: Found empty cache function %s\n", __func__, name);
            return (void*)GetProcAddress(hModule, name);
        }
    }
    return NULL;
}

bool torch_init() {
    // 1. Get a handle to the already-loaded DLL
    // PyTorch on Windows uses 'c10_cuda.dll' instead of 'libc10_cuda.so'
    HMODULE hModule = GetModuleHandleA("c10_cuda.dll");
 
    if (hModule == NULL) {
        log(ERROR, "%s: c10_cuda.dll not found in process memory. Running without on-fly GC. This lowers your usable VRAM.\n", __func__);
        return true;
    }

    // 2. Find the symbol
    // WARNING: Windows symbols are decorated differently. 
    // You must check the exports of c10_cuda.dll to get the exact name.
    empty_cache = find_torch_symbol(hModule);

    if (!empty_cache) {
        log(ERROR, "%s: c10_cuda.dll does not contain emptyCache. Running without on-fly GC. This lowers your usable VRAM.\n", __func__);
    }
    log(DEBUG, "%s: Torch empty_cache function found\n", __func__);
    return true;
}

