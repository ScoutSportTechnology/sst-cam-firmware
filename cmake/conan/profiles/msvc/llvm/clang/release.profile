[settings]
os=Windows
arch=x86_64
compiler=clang
compiler.version=21
compiler.cppstd=23
compiler.libcxx=libc++
build_type=Release


[buildenv]
PATH+=(path)C:/msys64/clang64/bin

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:compiler_executables={"c":"C:/msys64/clang64/bin/clang.exe","cpp":"C:/msys64/clang64/bin/clang++.exe"}
tools.compilation:verbosity=verbose
tools.microsoft.bash:subsystem=msys2
tools.microsoft.bash:path=C:/msys64/usr/bin/bash.exe
tools.microsoft.bash:active=False

[tool_requires]
ninja/[*]