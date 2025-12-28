import torch
import torch.nn.functional as F
from aimdo.model_vbar import ModelVBAR, vbar_fault, vbar_unpin, get_lib_path

def run_layer(input_tensor, weight_tensor, cpu_source):
    o = weight_tensor.model_vbar_offset
    if vbar_fault(weight_tensor):
        # SUCCESS: VBAR Weight is in VRAM        
        if not getattr(weight_tensor, 'is_populated', False):
            weight_tensor.copy_(cpu_source) 
            weight_tensor.is_populated = True
            print(f"[First Load] Populated 40MB weight at offset: {o}")
        else:
            print(f"[Secondary Fault] Reusing 40MB weight at offset: {o}")
        w = weight_tensor
    else:
        # FAIL: VBAR is under pressure (offloaded)
        print(f"[Offloaded] offset: {o}")
        w = cpu_source.to("cuda:0", non_blocking=True)

    #Layer math here
    output = input_tensor + w

    if w is weight_tensor:
        vbar_unpin(weight_tensor)

    return output

#python does some wild stuff with weakrefs and garbage collection here, so
#whatever you do, do not wrap these in a function.
allocator = torch.cuda.memory.CUDAPluggableAllocator(get_lib_path(), "alloc_fn", "free_fn")
pool = torch.cuda.MemPool(allocator.allocator())

#This installs aimdo to the pytorch main allocator
with torch.cuda.use_mem_pool(pool):
    #FIXME: prime torch properly somewhere else
    x = torch.randn(1, device=torch.device("cuda:0"))

    # --- Setup ---
    # 50GB VBAR

    vbar = ModelVBAR(14 * 1024**3, device=0)

    dtype = torch.float16
    # ~400MB weights
    shape = (10240, 20480) 
    num_layers = 12 * 1024 **3 // (20480 * 10240 * dtype.itemsize)

    print(f"allocating {num_layers} 400MB layers")

    weights = [vbar.alloc(shape, dtype=torch.float16) for _ in range(num_layers)]
    #just share one weight in this example
    #in the real world this will be separate weights for every layer
    cpu_weight = torch.randn(shape, dtype=torch.float16)

    x = torch.zeros(shape, device="cuda:0", dtype=torch.float16)

    # --- Start Inference ---
    vbar.prioritize() # Called ONCE at the start of the job

    for i in range(10): # Iteration loop
        print(f"\nIteration {i}")
        
        for layer_idx in range(num_layers):
            x = run_layer(x, weights[layer_idx], cpu_weight)

    print("\nFinal output shape:", x.shape)
