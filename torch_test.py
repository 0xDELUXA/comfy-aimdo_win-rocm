import os
import torch
import time

def memory_stress_test(chunk_size_mb=300):
    """
    Allocates tensors of a specified size until a CUDA OutOfMemoryError is caught.
    """
    
    bytes_per_mb = 1024 * 1024
    chunk_size_bytes = chunk_size_mb * bytes_per_mb
    
    dtype = torch.float32
    element_size = torch.finfo(dtype).bits // 8
    num_elements = chunk_size_bytes // element_size
    
    allocations = []
    cumulative_gb = 0.0
    
    device = torch.device("cuda:0")
    print(f"--- Stress Test Starting on Device: {device} ---")
    print(f"Attempting to allocate {chunk_size_mb} MB tensors (dtype: {dtype}).")

    try:
        while True:
            tensor_chunk = torch.empty(
                num_elements, 
                dtype=dtype, 
                device=device
            )
            allocations.append(tensor_chunk)
            
            cumulative_gb += chunk_size_mb / 1024.0
            print(f"Allocated {len(allocations):>4} tensors. Cumulative total: {cumulative_gb:.2f} GB")

    except RuntimeError as e:
        if "out of memory" in str(e):
            print("\n!!! CAUGHT EXCEPTION !!!")
            print(f"CUDA/Runtime Error: {e}")
            print(f"Max memory allocated before failure: {cumulative_gb:.2f} GB ({len(allocations)} chunks)")
        else:
            raise
    
    finally:
        print("\n--- Cleanup and Exit Sequence ---")
        print("Sleeping for 5 seconds (Check Task Manager etc) ...")
        time.sleep(5)
        print("Test finished.")
        del allocations
        torch.cuda.empty_cache()


if __name__ == "__main__":
    #x = torch.empty((1024, 1024, 1024, 4), dtype=torch.float32, device=torch.device("cuda:0"))
    memory_stress_test(chunk_size_mb=1000)

    current_dir = os.path.dirname(os.path.abspath(__file__))
    dll_path = os.path.join(current_dir, 'foo.dll')
    allocator = torch.cuda.memory.CUDAPluggableAllocator(dll_path, 'alloc_fn', 'free_fn').allocator()
    with torch.cuda.use_mem_pool(torch.cuda.memory.MemPool(allocator)):
        memory_stress_test(chunk_size_mb=1000)
