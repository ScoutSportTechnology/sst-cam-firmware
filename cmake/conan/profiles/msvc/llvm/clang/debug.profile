[settings]
os=Windows
arch=x86_64
build_type=Debug
compiler=clang
compiler.version=20
compiler.cppstd=23
compiler.runtime=dynamic
compiler.runtime_type=Debug
compiler.runtime_version=v145

[buildenv]
PATH+=(path)C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin


[conf]
tools.cmake.cmaketoolchain:generator=Visual Studio 18
tools.compilation:verbosity=verbose

[tool_requires]
ninja/[*]
