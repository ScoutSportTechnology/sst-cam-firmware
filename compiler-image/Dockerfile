FROM ghcr.io/scoutsporttechnology/sst-cam-firmware-compiler:jetpack_7.2-jetson_r39.2-v2

USER root
ENV CONAN_HOME=/opt/conan-cache

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
