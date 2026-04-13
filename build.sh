#!/bin/bash -eu
# Copyright 2026 Lawrence Livermore National Security, LLC
# Build script for ClusterFuzzLite integration
#
# This script is executed by ClusterFuzzLite to build fuzzing harnesses.
# Environment variables set by ClusterFuzzLite:
#   $CC, $CXX         - Compiler (AFL, libFuzzer, or honggfuzz wrapper)
#   $CFLAGS, $CXXFLAGS - Instrumentation flags
#   $LIB_FUZZING_ENGINE - Fuzzing engine library
#   $OUT              - Output directory for fuzzers
#   $SRC              - Source directory

# Install dependencies (only if not already installed by Dockerfile)
if ! command -v autoconf &> /dev/null; then
    apt-get update && apt-get install -y \
        autoconf \
        automake \
        libtool \
        pkg-config \
        libjansson-dev \
        libsodium-dev \
        uuid-dev \
        libmunge-dev
fi

cd $SRC/flux-security

# Generate configure script
./autogen.sh

# Configure with fuzzing enabled
./configure \
    --enable-fuzzing \
    CFLAGS="$CFLAGS" \
    CXXFLAGS="$CXXFLAGS"

# Build fuzzers
make -j$(nproc) fuzzers

# Copy fuzzers to $OUT
cp src/fuzz/fuzz_sign_unwrap $OUT/
cp src/fuzz/fuzz_sign_unwrap_noverify $OUT/
cp src/fuzz/fuzz_kv $OUT/
cp src/fuzz/fuzz_cf $OUT/

# Copy shared libraries to $OUT
# Copy project libraries
for lib in src/lib/.libs/*.so* src/libutil/.libs/*.so* src/libtomlc99/.libs/*.so* src/libca/.libs/*.so*; do
    [ -f "$lib" ] && cp "$lib" $OUT/
done

# Copy system dependencies using ldd
for fuzzer in src/fuzz/fuzz_*; do
    [ -x "$fuzzer" ] || continue
    ldd "$fuzzer" 2>/dev/null | grep "=>" | awk '{print $3}' | while read libpath; do
        [ -n "$libpath" ] && [ -f "$libpath" ] && cp -L "$libpath" $OUT/ 2>/dev/null || true
    done
done

# Set RPATH so fuzzers find libraries in their own directory
for fuzzer in $OUT/fuzz_*; do
    [ -f "$fuzzer" ] && patchelf --set-rpath '$ORIGIN' "$fuzzer"
done

# Copy corpus files (seed inputs for fuzzing)
# Each fuzzer gets its own corpus directory
mkdir -p $OUT/fuzz_sign_unwrap_seed_corpus
mkdir -p $OUT/fuzz_sign_unwrap_noverify_seed_corpus
mkdir -p $OUT/fuzz_kv_seed_corpus
mkdir -p $OUT/fuzz_cf_seed_corpus

# Generate seed corpus from test cases if available
# These give the fuzzer a good starting point
if [ -d "$SRC/flux-security/src/fuzz/corpus" ]; then
    # Copy existing corpus files
    find "$SRC/flux-security/src/fuzz/corpus" -type f -name "*.sign" \
        -exec cp {} $OUT/fuzz_sign_unwrap_seed_corpus/ \; 2>/dev/null || true
    find "$SRC/flux-security/src/fuzz/corpus" -type f -name "*.sign" \
        -exec cp {} $OUT/fuzz_sign_unwrap_noverify_seed_corpus/ \; 2>/dev/null || true
    find "$SRC/flux-security/src/fuzz/corpus" -type f -name "*.kv" \
        -exec cp {} $OUT/fuzz_kv_seed_corpus/ \; 2>/dev/null || true
    find "$SRC/flux-security/src/fuzz/corpus" -type f -name "*.toml" \
        -exec cp {} $OUT/fuzz_cf_seed_corpus/ \; 2>/dev/null || true
fi

# Create minimal seed corpus if no corpus exists
# ClusterFuzzLite works better with at least one seed input
if [ ! "$(ls -A $OUT/fuzz_sign_unwrap_seed_corpus)" ]; then
    echo "HEADER.PAYLOAD.SIGNATURE" > $OUT/fuzz_sign_unwrap_seed_corpus/minimal.txt
fi

if [ ! "$(ls -A $OUT/fuzz_sign_unwrap_noverify_seed_corpus)" ]; then
    echo "HEADER.PAYLOAD.SIGNATURE" > $OUT/fuzz_sign_unwrap_noverify_seed_corpus/minimal.txt
fi

if [ ! "$(ls -A $OUT/fuzz_kv_seed_corpus)" ]; then
    # Minimal KV structure: 4 bytes header + 1 null-terminated key + 1 byte value
    printf '\x06\x00\x00\x00k\x00\x00' > $OUT/fuzz_kv_seed_corpus/minimal.kv
fi

if [ ! "$(ls -A $OUT/fuzz_cf_seed_corpus)" ]; then
    echo "[test]" > $OUT/fuzz_cf_seed_corpus/minimal.toml
    echo 'key = "value"' >> $OUT/fuzz_cf_seed_corpus/minimal.toml
fi

# Copy configuration file needed by fuzzers
# Some fuzzers require conf.d/sign.toml to initialize
mkdir -p $OUT/conf.d
if [ -f "$SRC/flux-security/src/fuzz/conf.d/sign.toml" ]; then
    cp "$SRC/flux-security/src/fuzz/conf.d/sign.toml" $OUT/conf.d/
else
    # Create minimal config if not present
    cat > $OUT/conf.d/sign.toml << 'EOF'
[sign]
max-ttl = 1200
default-type = "none"
allowed-types = [ "none" ]
EOF
fi

# Set environment for fuzzers to find config
# The harnesses look for FUZZ_CONFIG_PATH
echo "export FUZZ_CONFIG_PATH=$OUT/conf.d/*.toml" > $OUT/fuzz_env.sh

echo "Build completed successfully!"
echo "Fuzzers built:"
ls -lh $OUT/fuzz_*
echo ""
echo "Corpus seeds:"
for dir in $OUT/*_seed_corpus; do
    echo "  $(basename $dir): $(ls $dir | wc -l) files"
done
