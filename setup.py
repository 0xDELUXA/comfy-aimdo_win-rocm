import os
import sys
from setuptools import setup, Extension

if sys.platform == "win32":
    # Windows paths (provided by the Jimver/cuda-toolkit action)
    cuda_path = os.environ.get("CUDA_PATH", "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.1")
    include_dirs = [os.path.join(cuda_path, "include")]
    library_dirs = [os.path.join(cuda_path, "lib", "x64")]
    libraries = ["cuda"]  # or "cudart" depending on what you need
else:
    # Your existing Linux paths
    include_dirs = ["/usr/local/cuda-12.1/include"]
    library_dirs = ["/usr/local/cuda-12.1/targets/x86_64-linux/lib/stubs"]
    libraries = ["cuda"]

setup(
    ext_modules=[
        Extension(
            name="aimdo.libvbar",
            sources=["src/aimdo.c"],
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            libraries=libraries,
        )
    ],
)
