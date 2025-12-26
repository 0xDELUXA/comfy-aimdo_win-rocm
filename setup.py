from setuptools import setup, Extension
import os

cuda_include = "/usr/local/cuda-12.1/include"
cuda_lib_stubs = "/usr/local/cuda-12.1/targets/x86_64-linux/lib/stubs"

libvbar = Extension(
    name="aimdo.libvbar",  # This will output libvbar.so inside the aimdo folder
    sources=["src/aimdo.c"],
    include_dirs=[cuda_include, "src"],
    library_dirs=[cuda_lib_stubs],
    libraries=["cuda"],
    extra_compile_args=["-fPIC"],
)

setup(
    ext_modules=[libvbar],
)
