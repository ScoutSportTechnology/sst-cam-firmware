[settings]
os=Linux
arch=x86_64
compiler=gcc
compiler.version=11
compiler.libcxx=libstdc++11

# IMPORTANT:
# Do NOT set build_type here either; let presets drive it.

[conf]
tools.cmake.cmaketoolchain:generator=Ninja