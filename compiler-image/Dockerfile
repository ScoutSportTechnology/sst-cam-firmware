# sst-cam-firmware-compiler — everything CI needs to cross-build, lint-build and
# test the firmware for JetPack 7.2 (L4T r39.2), and nothing else:
#   - /l4t/targetfs: the aarch64 sysroot (BSP sample rootfs + noble -dev debs +
#     the JetPack AI stack: CUDA 13.2, cuDNN 9.20, TensorRT 10.16, VPI 4.1),
#   - Ubuntu's gcc-13 aarch64 cross toolchain, cmake, ninja, conan,
#   - host protoc (proto codegen) and qemu-user-static (ctest runs the aarch64
#     test binaries under qemu).
# Editor/agent tooling (clangd, clang-format, clang-tidy, sudo) lives in the
# devcontainer (.devcontainer/Dockerfile), which builds FROM this image.
#
# Published by .github/workflows/compiler-image.yml as
#   ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:<VERSION> (+ :latest)
# VERSION (compiler-image/VERSION) = jetpack_<JetPack>-jetson_r<L4T>-v<n>.
# Any change in compiler-image/ needs a new VERSION: bump v<n>, or the JetPack /
# L4T part when moving the BSP. Published tags are never overwritten.
#
# Two stages on a digest-pinned base. `l4t` runs all sysroot steps in ONE layer,
# so only the finished, pruned sysroot is cached; the final stage COPYs it.
# To bump the base: docker buildx imagetools inspect ubuntu:24.04, update both FROMs.
FROM ubuntu:24.04@sha256:24737cef17f4e22b1977df7d9a05e77c4e28b7da5b8911f8d8ccda136bf3573c AS l4t

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        wget \
        curl \
        lbzip2 \
        qemu-user-static \
    && rm -rf /var/lib/apt/lists/*

# L4T_LOCAL_CACHE lets a local build mount pre-downloaded BSP tarballs (see 001).
ARG L4T_LOCAL_CACHE=""
ENV L4T_LOCAL_CACHE=${L4T_LOCAL_CACHE}

# Sysroot assembly, order-sensitive:
#   001 fetch the L4T r39.2 BSP + sample rootfs, apply_binaries (arm64 chroot, qemu)
#   004 JetPack 7.2 AI stack via apt in the pristine rootfs (pinned to the 7.2 GA set)
#   003 Ubuntu noble arm64 -dev debs the sample rootfs lacks (before 002's fixes)
#   002 pkg-config, symlink and crt fixes
#   005 prune what a cross sysroot never reads (BSP flashing payload, docs, ...)
COPY sysroot/ /tmp/sysroot/
RUN set -eu; \
    bash /tmp/sysroot/001_fetch_bsp_rootfs.sh /l4t; \
    bash /tmp/sysroot/004_install_ai_stack.sh /l4t/targetfs; \
    bash /tmp/sysroot/003_install_extra_pkgs.sh /l4t/targetfs; \
    bash /tmp/sysroot/002_fix_sysroot.sh /l4t/targetfs; \
    bash /tmp/sysroot/005_prune_sysroot.sh /l4t; \
    rm -rf /tmp/sysroot

# ══ Final image ═══════════════════════════════════════════════════════════════
FROM ubuntu:24.04@sha256:24737cef17f4e22b1977df7d9a05e77c4e28b7da5b8911f8d8ccda136bf3573c

LABEL org.opencontainers.image.source="https://github.com/ScoutSportTechnology/sst-cam-firmware" \
      org.opencontainers.image.description="sst-cam firmware cross-compiler: JetPack 7.2 / L4T r39.2 aarch64 sysroot + gcc-13 cross toolchain"

COPY --from=l4t /l4t /l4t

# Host build tools + cross toolchain. build-essential: Conan builds some deps with
# the host compiler. noble ships cmake 3.28 (>= 3.24).
RUN apt-get update && apt-get install -y --no-install-recommends \
        git \
        cmake \
        ninja-build \
        build-essential \
        pkg-config \
        pipx \
        protobuf-compiler \
        gcc-13-aarch64-linux-gnu \
        g++-13-aarch64-linux-gnu \
        ca-certificates \
        curl \
        wget \
        qemu-user-static \
    && rm -rf /var/lib/apt/lists/*

# Conan, system-wide so both CI (runs as root) and the `builder` user find it.
ENV PIPX_HOME=/opt/pipx \
    PIPX_BIN_DIR=/usr/local/bin
RUN pipx install conan==2.34.0

# Generic non-root user at uid/gid 1000 (matches the usual host user, so
# bind-mounted files stay writable). ubuntu:24.04 ships 'ubuntu' at 1000.
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN (userdel -r ubuntu 2>/dev/null || true) \
    && (groupdel ubuntu 2>/dev/null || true) \
    && groupadd --gid ${USER_GID} builder \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash builder

# Cross-compile paths (single aarch64-linux-gnu triple, Ubuntu gcc-13 cross).
ENV CROSS_SYSROOT=/l4t/targetfs \
    CROSS_TRIPLE=aarch64-linux-gnu \
    CROSS_GCC_SUFFIX=-13

USER builder
WORKDIR /workspace
