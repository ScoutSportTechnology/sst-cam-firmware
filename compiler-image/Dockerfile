FROM ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:jetpack_7.2-jetson_r39.2-v2

USER root
ENV CONAN_HOME=/opt/conan-cache

RUN set -eu; \
    deb=/tmp/nvidia-l4t-jetson-multimedia-api_39.2.0-20260601141651_arm64.deb; \
    curl -fSL --retry 5 -o "$deb" https://repo.download.nvidia.com/jetson/common/pool/main/n/nvidia-l4t-jetson-multimedia-api/nvidia-l4t-jetson-multimedia-api_39.2.0-20260601141651_arm64.deb; \
    printf '%s  %s\n' e8e6b2d0541cd0156d5e677a9bfb2173af9c3a2510327fb0a9c7539a83c4216d "$deb" | sha256sum -c -; \
    mkdir -p /tmp/mmapi; \
    dpkg-deb -x "$deb" /tmp/mmapi; \
    sdk=/tmp/mmapi/usr/src/jetson_multimedia_api; \
    target=/l4t/targetfs/usr/include; \
    cp -a "$sdk/argus/include/Argus" "$target/"; \
    cp -a "$sdk/include/nvbufsurface.h" "$target/"; \
    test -e /l4t/targetfs/usr/lib/aarch64-linux-gnu/tegra/libnvargus.so; \
    test -e /l4t/targetfs/usr/lib/aarch64-linux-gnu/tegra/libnvbufsurface.so; \
    test -e /l4t/targetfs/usr/lib/aarch64-linux-gnu/libEGL.so; \
    test -e /l4t/targetfs/usr/local/cuda-13.2/targets/sbsa-linux/include/cudaEGL.h; \
    rm -rf /tmp/mmapi "$deb"

COPY conanfile.py /tmp/conan-deps/conanfile.py
COPY conan/profiles/ /tmp/conan-deps/conan/profiles/
RUN set -eu; \
    for preset in Debug Release RelWithDebInfo; do \
      if [ "$preset" = RelWithDebInfo ]; then options='-o with_tests=True'; else options=''; fi; \
      conan install /tmp/conan-deps -of=/tmp/conan-deps/output \
        --profile:host=/tmp/conan-deps/conan/profiles/jetpack-7.2-cross \
        --profile:build=/tmp/conan-deps/conan/profiles/cross \
        -s "build_type=$preset" --build=missing $options; \
    done; \
    rm -rf /tmp/conan-deps; \
    chroot /l4t/targetfs /usr/bin/fc-cache -f; \
    chown -R builder:builder /opt/conan-cache

USER builder
WORKDIR /workspace
