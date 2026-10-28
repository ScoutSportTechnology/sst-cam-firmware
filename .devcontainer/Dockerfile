# Firmware devcontainer = the published compiler image + editor tooling.
#
# The sysroot, cross toolchain, cmake/conan/protoc/qemu all come from
# ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler (recipe: compiler-image/).
# Nothing heavy is built locally; this only adds the tools used while editing.
# Keep the tag equal to compiler-image/VERSION (CI checks it).
FROM ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:jetpack_7.2-jetson_r39.2-v1

USER root

# clangd (LSP), clang-format + clang-tidy (the same noble v18 the CI gates use),
# sudo for ad-hoc installs inside the container.
RUN apt-get update && apt-get install -y --no-install-recommends \
        clangd \
        clang-format \
        clang-tidy \
        sudo \
    && rm -rf /var/lib/apt/lists/* \
    && echo 'builder ALL=(root) NOPASSWD:ALL' > /etc/sudoers.d/builder \
    && chmod 0440 /etc/sudoers.d/builder

USER builder
WORKDIR /workspace
