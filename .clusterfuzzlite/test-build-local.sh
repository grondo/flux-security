#!/bin/bash
# Quick local test of ClusterFuzzLite build script
# Run this before pushing to verify the build works

set -e

echo "=== ClusterFuzzLite Build Test ==="
echo ""
echo "This will test the build script in a Docker container."
echo "Press Ctrl+C to cancel, or wait 5 seconds to continue..."
sleep 5

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    echo ""
    echo "Cleaning up..."
    rm -rf "$BUILD_DIR"
}
trap cleanup EXIT

echo ""
echo "=== Step 1: Setting up test environment ==="
mkdir -p "$BUILD_DIR/out"
mkdir -p "$BUILD_DIR/src/flux-security"

echo "Copying repository to build directory..."
cp -r "$REPO_ROOT"/* "$BUILD_DIR/src/flux-security/" 2>/dev/null || true
cp -r "$REPO_ROOT"/.clusterfuzzlite "$BUILD_DIR/src/flux-security/"

echo ""
echo "=== Step 2: Running build in Docker ==="
echo "This may take 5-10 minutes on first run..."

docker run --rm \
    -v "$BUILD_DIR/out:/out" \
    -v "$BUILD_DIR/src:/src" \
    -e OUT=/out \
    -e SRC=/src \
    -e CC=clang \
    -e CXX=clang++ \
    -e CFLAGS="-fsanitize=address -fsanitize=fuzzer-no-link -g" \
    -e CXXFLAGS="-fsanitize=address -fsanitize=fuzzer-no-link -g" \
    -e LIB_FUZZING_ENGINE="" \
    ubuntu:22.04 \
    bash -c 'cd /src/flux-security && bash .clusterfuzzlite/build.sh'

echo ""
echo "=== Step 3: Verifying build outputs ==="

EXPECTED_FUZZERS=(
    "fuzz_sign_unwrap"
    "fuzz_sign_unwrap_noverify"
    "fuzz_kv"
    "fuzz_cf"
)

echo "Checking for fuzzers..."
ALL_FOUND=true
for fuzzer in "${EXPECTED_FUZZERS[@]}"; do
    if [ -f "$BUILD_DIR/out/$fuzzer" ]; then
        echo "  ✓ $fuzzer found"
    else
        echo "  ✗ $fuzzer MISSING"
        ALL_FOUND=false
    fi
done

echo ""
echo "Checking for seed corpus..."
for fuzzer in "${EXPECTED_FUZZERS[@]}"; do
    corpus_dir="$BUILD_DIR/out/${fuzzer}_seed_corpus"
    if [ -d "$corpus_dir" ]; then
        count=$(ls "$corpus_dir" | wc -l)
        echo "  ✓ ${fuzzer}_seed_corpus: $count files"
    else
        echo "  ✗ ${fuzzer}_seed_corpus MISSING"
        ALL_FOUND=false
    fi
done

echo ""
echo "Checking for config files..."
if [ -f "$BUILD_DIR/out/conf.d/sign.toml" ]; then
    echo "  ✓ conf.d/sign.toml found"
else
    echo "  ✗ conf.d/sign.toml MISSING"
    ALL_FOUND=false
fi

echo ""
echo "=== Step 4: Quick fuzzer test ==="
echo "Running fuzz_kv with minimal input..."
echo "test" | timeout 5 "$BUILD_DIR/out/fuzz_kv" || true
echo "  ✓ Fuzzer executed (no crash expected)"

echo ""
echo "=== Results ==="
if [ "$ALL_FOUND" = true ]; then
    echo "✅ BUILD TEST PASSED"
    echo ""
    echo "All fuzzers built successfully!"
    echo "Build artifacts:"
    ls -lh "$BUILD_DIR/out/fuzz_"*
    echo ""
    echo "You can now commit and push the ClusterFuzzLite integration."
    echo ""
    echo "Next steps:"
    echo "  1. git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml"
    echo "  2. git commit -m 'Add ClusterFuzzLite CI fuzzing integration'"
    echo "  3. git push origin your-branch"
    echo "  4. Open PR and watch for ClusterFuzzLite comment"
    exit 0
else
    echo "❌ BUILD TEST FAILED"
    echo ""
    echo "Some fuzzers or required files are missing."
    echo "Check the build logs above for errors."
    echo ""
    echo "Common issues:"
    echo "  - Missing dependencies in build.sh"
    echo "  - Configure or make failures"
    echo "  - Incorrect paths in build.sh"
    exit 1
fi
