[settings]
os=Windows
arch=x86_64
compiler=clang
build_type=Debug
compiler.version=17
compiler.cppstd=23
compiler.libcxx=libc++

[buildenv]
PATH+=(path)C:/msys64/clang64/bin
PATH+=(path)C:/msys64/usr/bin

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:compiler_executables = {"c": "clang", "cpp": "clang++"}
#tools.microsoft.bash:subsystem=msys2