# ClusterFuzzLite Dockerfile for flux-security
# All fuzzers support both AFL++ persistent mode and libFuzzer entry points
# via conditional compilation. Build script is at project root (build.sh).

FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    libtool \
    pkg-config \
    libjansson-dev \
    libsodium-dev \
    uuid-dev \
    libmunge-dev \
    patchelf \
    && rm -rf /var/lib/apt/lists/*

COPY . $SRC/flux-security
# ClusterFuzzLite expects build script at $SRC/build.sh
COPY build.sh $SRC/build.sh
WORKDIR $SRC/flux-security
