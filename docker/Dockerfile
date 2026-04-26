FROM nvcr.io/nvidia/jetpack-linux-aarch64-crosscompile-x86:6.1

# ── Install build tools ────────────────────────────────────────────────────────
# Add Kitware repo to get CMake >= 3.24 (Ubuntu 22.04 ships 3.22).
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        wget \
        gpg \
        lbzip2 \
        ninja-build \
        python3-pip \
        pipx \
        pkg-config \
        clangd \
    && wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc \
       | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' \
       > /etc/apt/sources.list.d/kitware.list \
    && apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

RUN pipx install conan
ENV PATH="/root/.local/bin:${PATH}"

# ── 001: Unpack toolchain + sysroot tarballs from the NGC base image ─────────
COPY docker/scripts/001_unpack_l4t.sh /tmp/001_unpack_l4t.sh
RUN bash /tmp/001_unpack_l4t.sh /l4t && rm /tmp/001_unpack_l4t.sh

# ── 002: Fix sysroot pkg-config, symlinks, crt stubs ─────────────────────────
COPY docker/scripts/002_fix_sysroot.sh /tmp/002_fix_sysroot.sh
RUN bash /tmp/002_fix_sysroot.sh /l4t/targetfs && rm /tmp/002_fix_sysroot.sh

# ── Export cross-compile paths as env vars ─────────────────────────────────────
ENV CROSS_SYSROOT=/l4t/targetfs
ENV BOOTLIN_TOOLCHAIN_BIN=/l4t/aarch64--glibc--stable-2022.08-1/bin

WORKDIR /workspace
