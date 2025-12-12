[settings]
os=Windows
arch=x86_64
compiler=clang
build_type=Debug
compiler.version=21
compiler.cppstd=23
compiler.libcxx=libc++

[buildenv]
PATH+=(path)C:/msys64/clang64/bin

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:compiler_executables={"c":"C:/msys64/clang64/bin/clang.exe","cpp":"C:/msys64/clang64/bin/clang++.exe"}