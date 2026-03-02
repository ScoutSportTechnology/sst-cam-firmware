# ------------------------------------------------------------
# Jetson Orin AArch64 Cross Toolchain
# Target: JetPack 6.2.2 (L4T 36.5.0)
#
# Expected environment (set by bootstrap-cross-compiling.sh):
#   SYSROOT=/.../targetfs
#   CROSS_COMPILE=aarch64-buildroot-linux-gnu-
#   CROSS_COMPILE_AARCH64_PATH=/.../aarch64--glibc--stable-2022.08-1
# ------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ---- Sysroot (prefer env from bootstrap, fallback to cache path) ----
if(DEFINED ENV{SYSROOT})
  set(CMAKE_SYSROOT "$ENV{SYSROOT}")
elseif(DEFINED ENV{TARGET_ROOTFS})
  set(CMAKE_SYSROOT "$ENV{TARGET_ROOTFS}")
else()
  set(CMAKE_SYSROOT "/.cache/nvidia-l4t/targetfs")
endif()

# ---- Toolchain prefix (prefer env from bootstrap) ----
# NVIDIA JetPack 6 cross container uses aarch64-buildroot-linux-gnu-
if(DEFINED ENV{CROSS_COMPILE})
  set(_CROSS_PREFIX "$ENV{CROSS_COMPILE}")
elseif(DEFINED ENV{CROSS_COMPILE_AARCH64})
  set(_CROSS_PREFIX "$ENV{CROSS_COMPILE_AARCH64}")
else()
  set(_CROSS_PREFIX "aarch64-buildroot-linux-gnu-")
endif()

# ---- Compilers ----
# Use the prefix directly; CMake accepts full executable names.
set(CMAKE_C_COMPILER   "${_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${_CROSS_PREFIX}g++")

# Ensure sysroot is actually used
set(CMAKE_C_FLAGS_INIT              "--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT            "--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT     "--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT  "--sysroot=${CMAKE_SYSROOT}")

# ---- Tell CMake where to search for headers/libs/packages ----
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---- pkg-config (critical to avoid x86 host contamination) ----
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR}
  "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:"
  "${CMAKE_SYSROOT}/usr/lib/pkgconfig:"
  "${CMAKE_SYSROOT}/usr/share/pkgconfig:"
  "${CMAKE_SYSROOT}/usr/local/lib/aarch64-linux-gnu/pkgconfig:"
  "${CMAKE_SYSROOT}/usr/local/lib/pkgconfig"
)

# Avoid CMake trying to run target binaries during configure
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---- Optional: nice-to-have hints for CUDA/TensorRT/VPI inside sysroot ----
# These don't "install" anything; they just help find_package() in some projects.
if(EXISTS "${CMAKE_SYSROOT}/usr/local/cuda")
  set(CMAKE_CUDA_COMPILER_FORCED TRUE)
  set(CUDA_TOOLKIT_ROOT_DIR "${CMAKE_SYSROOT}/usr/local/cuda" CACHE PATH "" FORCE)
endif()

# TensorRT typically lives in /usr/lib/aarch64-linux-gnu and /usr/include
set(TENSORRT_ROOT "${CMAKE_SYSROOT}/usr" CACHE PATH "" FORCE)

# VPI on JetPack 6 is usually /opt/nvidia/vpi3
if(EXISTS "${CMAKE_SYSROOT}/opt/nvidia/vpi3")
  set(VPI_ROOT "${CMAKE_SYSROOT}/opt/nvidia/vpi3" CACHE PATH "" FORCE)
endif()