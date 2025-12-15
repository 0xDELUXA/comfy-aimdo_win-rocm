#include <stdio.h>
#include <stdlib.h>
// CHANGE: Use cuda_runtime.h for Runtime API functions
#include <cuda_runtime.h>

// -------------------------------------------------------------------------
// MISSING DEFINITION: Helper function for checking CUDA errors
// -------------------------------------------------------------------------
// Note: We use the actual function name 'check_cuda_errors' in the definition
// and redefine the macro 'checkCudaErrors' to call it.

#define checkCudaErrors(val) check_cuda_errors((val), #val, __FILE__, __LINE__)

void check_cuda_errors(cudaError_t result, const char* func, const char* file, int line) {
    if (result != cudaSuccess) {
        // Use fprintf and stderr defined in stdio.h
        fprintf(stderr, "CUDA Error at %s:%d: %s (%s)\n",
            file, line, cudaGetErrorString(result), func);
        exit(1);
    }
}
// -------------------------------------------------------------------------


int main(void) {
    int device_count = 0;
    int bytes_to_allocate = 1024;
    void* d_ptr = NULL;

    printf("--- CUDA C Memory Allocator ---\n");

    // 1. Check for available CUDA devices
    checkCudaErrors(cudaGetDeviceCount(&device_count)); // Call to the MACRO
    if (device_count == 0) {
        fprintf(stderr, "No CUDA devices found. Exiting.\n");
        return 1;
    }
    printf("Found %d CUDA device(s).\n", device_count);

    // 2. Allocate memory on the GPU (Device)
    printf("Attempting to allocate %d bytes on the GPU...\n", bytes_to_allocate);

    // The core allocation call
    checkCudaErrors(cudaMalloc(&d_ptr, bytes_to_allocate)); // Call to the MACRO

    if (d_ptr != NULL) {
        printf("Successfully allocated memory at device address: %p\n", d_ptr);
    }
    else {
        // Note: Error message already printed by checkCudaErrors
        printf("Allocation failed.\n");
        return 1;
    }

    // 3. Free the allocated memory
    checkCudaErrors(cudaFree(d_ptr)); // Call to the MACRO
    printf("Successfully freed device memory.\n");

    return 0;
}