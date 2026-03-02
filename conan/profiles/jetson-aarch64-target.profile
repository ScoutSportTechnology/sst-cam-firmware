[settings]
os=Linux
arch=armv8
compiler=gcc
compiler.version=11
compiler.libcxx=libstdc++11

# IMPORTANT:
# Do NOT set build_type here. Let CMakePresets control it via -s:h build_type=...

[conf]
# Cross-compilation hints for Conan's CMakeToolchain generator (if used by recipes)
tools.cmake.cmaketoolchain:system_name=Linux
tools.cmake.cmaketoolchain:system_processor=aarch64

# Must match your bootstrap sysroot location
tools.cmake.cmaketoolchain:sysroot=/.cache/nvidia-l4t/targetfs

# NVIDIA JP6 cross container toolchain prefix
tools.build:compiler_executables={"c":"aarch64-buildroot-linux-gnu-gcc","cpp":"aarch64-buildroot-linux-gnu-g++"}

# Consistent generator
tools.cmake.cmaketoolchain:generator=Ninja

# Nice-to-have for cross builds (prevents some recipes from trying to run target executables)
tools.build:cross_building=True