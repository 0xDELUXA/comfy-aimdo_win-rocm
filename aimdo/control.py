
import torch
import os
import ctypes
import platform
from pathlib import Path


class CUDAPluggableAllocator(torch.cuda.memory.CUDAPluggableAllocator):
    def __init__(self, lib, alloc_fn_name: str, free_fn_name: str):
        alloc_fn = ctypes.cast(getattr(lib, alloc_fn_name), ctypes.c_void_p).value
        free_fn = ctypes.cast(getattr(lib, free_fn_name), ctypes.c_void_p).value
        assert alloc_fn is not None
        assert free_fn is not None
        self._allocator = torch._C._cuda_customAllocator(alloc_fn, free_fn)

def get_lib_path():
    # Get the directory where this script/package is located
    base_path = Path(__file__).parent.resolve()

    # Determine extension based on OS
    system = platform.system()
    if system == "Windows":
        lib_name = "aimdo.dll"
    elif system == "Linux":
        lib_name = "aimdo.so"
    else:
        # MacOS usually uses .dylib, though often .so works
        lib_name = "aimdo.so"

    return str(base_path / lib_name)

# Load the library
lib_path = get_lib_path()
if not os.path.exists(lib_path):
    raise ImportError(f"Cannot find native library at {lib_path}")

lib=None
allocator=None

def init():
    global lib, allocator
    lib = ctypes.CDLL(lib_path)
    allocator = CUDAPluggableAllocator(lib, "alloc_fn", "free_fn")

