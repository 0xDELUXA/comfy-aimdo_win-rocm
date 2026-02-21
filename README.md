# AI Model Dynamic Offloader

This project is a pytorch VRAM allocator that implements on-demand offloading of model weights when the primary pytorch VRAM allocator comes under pressure.

## Support:

* **Nvidia GPUs only**
* **Pytorch 2.6+**
* **Cuda 12.8+**
* **Windows 11+** / **Linux** as per python ManyLinux support

---

## How it works:

* The pytorch application creates a Virtual Base Address Register (**VBAR**) for a model. Creating a VBAR doesn't cost any VRAM, only GPU virtual address space (which is pretty much free).
* The pytorch application allocates tensors for model weights within the VBAR. These tensors are initially un-allocated and will segfault if touched.
* The pytorch application faults in the tensors using the `fault()` API at the time the tensor is needed. This is where VRAM actually gets allocated.

##### If the `fault()` is successful (sufficient VRAM for this tensor):
1.  **If the fault() resultant signature is changed or unknown:**
    * The application uses `tensor::_copy()` to populate the weight data on the GPU.
    * The application saves the returned signature against this weight for future comparison
2.  The layer uses the weight tensor.
3.  The application calls `unpin()` on the tensor to allow it to be freed under pressure later if needed.

##### If the `fault()` is unsuccessful (offloaded weight):
1.  The application allocates a temporary regular GPU tensor.
2.  Uses `_copy` to populate weight data on the GPU.
3.  The layer uses the temporary as the weight.
4.  Pytorch garbage collects the temp when the layer is finished.

see examples/example.py

---

## Priorities:

* The most recent VBARs are the highest priority and lower addresses in the VBAR take priority over higher addresses.
* Applications should order their tensor allocations in the VBAR in load-priority order with the lowest addresses for the highest priority weights.
* Calling `fault()` on a weight that is higher priority than other weights will cause those lower priority weights to get freed to make space.
* Having a weight evicted sets that VBAR's watermark to that weight's level. Any weights in the same VBAR above the watermark automatically fail the `fault()` API. This avoids constantly faulting in all weights each model iteration while allowing the application to just blindly call `fault()` every layer and check the results. There is no need for the application to manage any VRAM quotas or watermarks.
* Existing VBARs can be pushed to top priority with the `prioritize()` API. This allows use of an already loaded or partially model (e.g. using the same model twice in a complex workflow). Using `prioritize` resets the offload watermark of that model to no offloading, giving its weights priority over any other currently loaded models.

---

## Backend:

* VBAR allocation is done with `cuMemAddressReserve()`, faulting with `cuMemCreate()` and `cuMemMap()` and all frees done with appropriate converse APIs.
* For consistency with VBAR memory management, main pytorch allocator plugin is also implemented with `cuMemAddressReserve` -> `cuMemCreate` -> `cuMemMap`. This also behaves a lot better on Windows systems with System Memory fallback.
* This allocator is incompatible with the pytorch `cudaMallocAsync` backend or expandable segments backends (as the plugin interface does not exist on these backends as of this writing).

## Caveats:

* There is no real way for this allocator to tell the difference between high usage and bad fragmentation in the pytorch caching allocator. As we always return success to the pytorch caching allocator it experiences no pressure while weights are being offloaded which means it can run in an extremely fragmented mode. The assumption is model weight access patterns are reasonably regular over blocks or iterations and it finds a good set of sizes to cache. What you should generally do though, is completely flush the pytorch caching allocator before each new model run, which avoids completely un-used reservations from taking priority over the next models weights.

## Experimental Windows ROCm ([TheRock](https://github.com/ROCm/TheRock)) support 

This fork adds a Windows batch script to build `comfy-aimdo` with ROCm support.

### Prerequisites

- ComfyUI
- ROCm (TheRock) SDK & PyTorch
- Visual Studio 2022 with the Desktop development with C++ workload
- CUDA toolkit (for hipify)
- Git

### Build and Install

1. Open PowerShell or Command Prompt.
2. Navigate to the folder where you want to clone the repository.

   For example:
```powershell
cd C:/
```

3. Clone this fork:
```powershell
git clone https://github.com/0xDELUXA/comfy-aimdo_win-rocm
```

4. Navigate to its directory:
```powershell
cd comfy-aimdo_win-rocm
```
- Alternatively, you can get the latest updates by cloning the upstream [repository](https://github.com/Comfy-Org/comfy-aimdo). In that case, you need to download the Windows ROCm build script into the cloned directory, and resolve potential conflicts:
```powershell
curl -O https://raw.githubusercontent.com/0xDELUXA/comfy-aimdo_win-rocm/refs/heads/master/build-rocm-windows.bat
```

5. Activate your ComfyUI virtual environment.

   (If you changed directories, return to the cloned `comfy-aimdo` folder before continuing.)

6. Run the batch script:
```powershell
.\build-rocm-windows.bat
```

7. - The script will automatically compile `aimdo.dll` along with `aimdo.lib` and modify the necessary files to be compatible with Windows ROCm. During the process, it will prompt you for the locations of the ROCm SDK core, CUDA Toolkit, and Visual Studio.
   - To install the built files, run `pip install .`
   - After install, you must manually copy `amdhip64_7.dll` from your ROCm SDK into your venv - the script will specify the required location

8. After completion, `comfy-aimdo` should work on Windows ROCm.

### Additional notes

- The "new" `Model Initializing...` phase introduced by `comfy-aimdo` can be quite demanding on AMD GPUs, particularly for larger models, and may even hang.
- If your ComfyUI startup console prints: `HIP Library Path: C:\WINDOWS\SYSTEM32\amdhip64_7.dll`, (at least on RDNA4 with Adrenaline 26.1.1) `comfy-aimdo` will not work at all. This is why the final manual copy step is required.
- For some reason, in certain workflows, `comfy-aimdo` breaks [`triton-windows`](https://github.com/triton-lang/triton-windows) on AMD with the following error:   `ValueError: Pointer argument (at 0) cannot be accessed from Triton (cpu tensor?)`.  As a result, we cannot use SageAttention V1 or FlashAttention-2 - only SDPA works (for now).

Tested on Windows 11 with the latest version of TheRock ROCm (`7.12.0a20260218`), PyTorch (`2.12.0a0+rocm7.12.0a20260218`) and an RDNA4 GPU (AMD Radeon RX 9060 XT) in latest ComfyUI (`v0.14.2`), launched with the `--fast` flag.

*This is experimental and may not function as expected. Even after a successful build, installation, and loading, occasional GPU hangs, out-of-memory (OOM) errors, and other runtime issues may occur on AMD hardware running Windows.*
