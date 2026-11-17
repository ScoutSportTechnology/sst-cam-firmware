# Firmware devcontainer = the published compiler image + editor tooling.
#
# The sysroot, cross toolchain, cmake/conan/protoc/qemu all come from
# ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler (recipe: compiler-image/).
# Nothing heavy is built locally; this only adds the tools used while editing.
# Keep the digest equal to compiler-image/DIGEST (CI checks it).
FROM ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler@sha256:75b5b0b55e5b93c61281abe57f4d41c0e79a87fb214a0912b741e075b3f97ee8

USER root

# clangd (LSP) and sudo for ad-hoc installs inside the container.
RUN apt-get update && apt-get install -y --no-install-recommends \
        clangd \
        sudo \
    && rm -rf /var/lib/apt/lists/* \
    && echo 'builder ALL=(root) NOPASSWD:ALL' > /etc/sudoers.d/builder \
    && chmod 0440 /etc/sudoers.d/builder

USER builder
WORKDIR /workspace
