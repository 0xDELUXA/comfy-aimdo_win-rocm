import os
import sys
from setuptools import setup, Distribution

# This trick forces the wheel to be labeled with the platform (e.g., win_amd64)
# instead of "any", which is required for binary DLLs.
class BinaryDistribution(Distribution):
    def has_ext_modules(foo): return True

setup(
    name="aimdo",
    version="0.1.0",
    packages=["aimdo"],
    # We include the compiled binary as package data
    package_data={
        "aimdo": ["aimdo.dll" if sys.platform == "win32" else "aimdo.so"],
    },
    distclass=BinaryDistribution,
)
