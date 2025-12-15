
cl.exe /c /I "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\include" foo.c
link.exe foo.obj /OUT:foo.exe /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\lib\x64" cudart.lib kernel32.lib
