#1d int8 tensor that the user can then view(dtype=).view(shape=) as whatever they want

def get_tensor_from_raw_ptr(ptr, device, size):

    container = {
        "shape": (size,),
        "typestr": "|u1",
        "data": (ptr, False), #writable
        "version": 3,
    }
    
    class Holder:
        pass
 
    holder = Holder()
    holder.__cuda_array_interface__ = container
    
    return torch.as_tensor(holder, device=device)

import ctypes

lib = ctypes.CDLL(...)

# void *vbar_allocate(uint64_t size);
lib.allocate_vbar.argtypes = [ctypes.c_uint64_t]
lib.allocate_vbar.restype = ctypes.c_void_p

#uint64_t vbar_get(void *vbar);
lib.get_vbar.argtypes = [ctypes.c_void_p]
lib.get_vbar.restype = ctypes.c_uint64_t

#void vbar_free(void *vbar);
lib.vbar_free.argtypes = [ctypes.c_void_p]
lib.vbar_free.restype = None

#bool vbar_fault(void *vbar, uint64_t offset, uint64_t size);
lib.vbar_fault.argtypes = [ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint64]
lib.vbar_fault.restype = ctypes.c_bool

class ModelVBAR:
    def __init__(self, size):
        self._ptr = lib.vbar_allocate(size)
        if not self._ptr:
            raise MemoryError("VBAR allocation failed")
        
        self.base_addr = lib.vbar_get(self._ptr)
        self.offset = 0
        self.max_size = size

    def alloc(self, shape):
        # Align offset to 512B
        self.offset = (self.offset + 511) & ~511
        
        num_bytes = torch.tensor([], dtype=dtype).element_size()
        for dim in shape:
            num_bytes *= dim

        if self.offset + num_bytes > self.max_size:
            raise MemoryError(f"VBAR OOM: {self.offset + num_bytes} > {self.max_size}")

        # Calculate absolute address for the tensor holder
        target_addr = self.base_addr + self.offset
        tensor = get_tensor_from_raw_ptr(target_addr, shape, dtype)

        tensor.model_vbar = self
        tensor.model_var_offset = self.offset
        
        self.offset += num_bytes
        return tensor

    def fault(self, offset, size):
        return lib.vbar_fault(self._ptr, offset, size)

    def __del__(self):
        if getattr(self, '_ptr', None):
            lib.vbar_free(self._ptr)
            self._ptr = None

