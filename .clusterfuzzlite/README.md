# ClusterFuzzLite Integration

This directory contains the ClusterFuzzLite configuration for automated fuzzing of flux-security.

## Overview

ClusterFuzzLite provides continuous fuzzing through GitHub Actions:

- **PR Fuzzing**: Runs 10 minutes on every pull request
- **Batch Fuzzing**: Runs 6 hours nightly with multiple sanitizers
- **Automatic Issue Creation**: Creates GitHub issues when crashes are found
- **Corpus Management**: Stores discovered inputs for regression testing

## Workflow Files

### `.github/workflows/clusterfuzzlite-pr.yml`
Runs quick fuzzing on every PR to catch regressions early.

**When it runs:**
- On every pull request
- Only when source code or fuzzing config changes

**What it does:**
- Builds all fuzzers with AddressSanitizer
- Runs 10 minutes of fuzzing across all harnesses
- Comments on PR with results
- Fails the PR if crashes are found

### `.github/workflows/clusterfuzzlite-batch.yml`
Runs comprehensive fuzzing campaigns nightly.

**When it runs:**
- Nightly at 2 AM UTC
- Manually via workflow_dispatch

**What it does:**
- Builds fuzzers with Address, Undefined, and Memory sanitizers
- Runs 6 hours of fuzzing per sanitizer
- Stores corpus for continuous fuzzing
- Creates GitHub issues for crashes
- Uploads crash artifacts

## Build Script

### `build.sh`
Builds fuzzing harnesses for ClusterFuzzLite.

**What it does:**
1. Installs dependencies (jansson, sodium, uuid, munge)
2. Runs autotools build with `--enable-fuzzing`
3. Copies fuzzers to ClusterFuzzLite output directory
4. Creates seed corpus from existing test inputs
5. Sets up configuration files needed by fuzzers

**Customization:**
- Edit dependency list if new libraries are added
- Update fuzzer list if new harnesses are created
- Modify corpus generation for better coverage

## Fuzzers

ClusterFuzzLite builds and runs these fuzzers:

1. **fuzz_sign_unwrap** - Full signature verification parser
   - Tests: Base64 decoding, header parsing, signature verification
   - Speed: Slower (~20-50k exec/sec) due to crypto
   
2. **fuzz_sign_unwrap_noverify** - Parser without crypto overhead
   - Tests: Base64 decoding, header parsing, payload extraction
   - Speed: Faster (~180k exec/sec)
   
3. **fuzz_kv** - KV format parser (privsep communication)
   - Tests: Key-value parsing, type handling, buffer boundaries
   - Speed: Very fast (~200k exec/sec)
   
4. **fuzz_cf** - TOML configuration parser
   - Tests: TOML parsing, schema validation, type coercion
   - Speed: Fast (~100-150k exec/sec)

## Sanitizers

ClusterFuzzLite uses multiple sanitizers to catch different bug classes:

- **AddressSanitizer (ASan)**: Buffer overflows, use-after-free, heap corruption
- **UndefinedBehaviorSanitizer (UBSan)**: Integer overflow, null pointer dereference
- **MemorySanitizer (MSan)**: Use of uninitialized memory

## Corpus Management

ClusterFuzzLite automatically manages corpus (interesting test inputs):

- **Storage**: Stored in `clusterfuzzlite-corpus` branch
- **Coverage**: Coverage reports in `gh-pages` branch
- **Persistence**: Corpus grows over time as new paths are discovered
- **Minimization**: Duplicate/redundant inputs are removed

## Viewing Results

### PR Fuzzing
Results appear as PR comments:
```
✅ No crashes found during 10-minute fuzzing session.

Fuzzers tested:
- fuzz_sign_unwrap - Signature verification parser
- fuzz_sign_unwrap_noverify - Fast parser (no crypto)
- fuzz_kv - KV format parser (privsep)
- fuzz_cf - TOML configuration parser
```

### Batch Fuzzing
Results appear as:
- GitHub issues (if crashes found)
- Workflow artifacts (crash files)
- Corpus updates (new interesting inputs)

## Handling Crashes

When ClusterFuzzLite finds a crash:

1. **PR Fuzzing**: PR is marked as failed, artifact contains crash
2. **Batch Fuzzing**: GitHub issue is created automatically

### Reproducing Crashes

```bash
# Download crash artifact from workflow run
# Extract crash file

# Build with sanitizer
CC=clang CFLAGS="-fsanitize=address -g" ./configure --enable-fuzzing
make

# Reproduce
FUZZ_DEBUG=1 src/fuzz/fuzz_sign_unwrap < crash-file

# Or use AFL tmin to minimize
afl-tmin -i crash-file -o minimized.txt -- ./src/fuzz/fuzz_sign_unwrap
```

### Fixing Crashes

1. Reproduce locally with sanitizers enabled
2. Debug with gdb/lldb
3. Fix the bug
4. Add regression test
5. Verify fix by re-running fuzzer

## Manual Testing

### Running ClusterFuzzLite Locally

```bash
# Build container
docker build -t clusterfuzzlite-local -f .clusterfuzzlite/Dockerfile .

# Run fuzzing
docker run --rm -v $(pwd):/src clusterfuzzlite-local

# Or use the helper script
.clusterfuzzlite/build.sh
```

### Integration with Existing Fuzzing

ClusterFuzzLite complements (doesn't replace) the existing AFL++ setup:

- **ClusterFuzzLite**: Automated, continuous, integrated with GitHub
- **AFL++**: Manual, longer campaigns, more control, local development

Both use the same harnesses, so bugs found by either are valid.

## Configuration Files

### `project.yaml`
Project-level configuration:
- Supported sanitizers
- Fuzzing engines
- Main fuzzers to run
- Contact information

### Corpus Seeds
Initial inputs for fuzzing:
- `*_seed_corpus/` - Starting test cases
- Built from existing test suite
- Minimal examples for each parser

## Troubleshooting

### Build Failures

**Problem**: Dependencies missing
```
Solution: Update apt-get install list in build.sh
```

**Problem**: Fuzzers not found in $OUT
```
Solution: Check that --enable-fuzzing is set and make succeeds
```

### Fuzzing Failures

**Problem**: Fuzzers crash immediately
```
Solution: Check that FUZZ_CONFIG_PATH is set correctly
          Verify conf.d/sign.toml exists
```

**Problem**: Low coverage
```
Solution: Add better seed corpus
          Check that sanitizers aren't slowing execution too much
```

### Workflow Failures

**Problem**: PR comment not appearing
```
Solution: Check GITHUB_TOKEN permissions (needs write:issues)
```

**Problem**: Corpus not persisting
```
Solution: Verify clusterfuzzlite-corpus branch exists
          Check storage-repo configuration
```

## References

- [ClusterFuzzLite Documentation](https://google.github.io/clusterfuzzlite/)
- [OSS-Fuzz](https://github.com/google/oss-fuzz) - Full continuous fuzzing platform
- [libFuzzer Tutorial](https://llvm.org/docs/LibFuzzer.html)
- [Sanitizers Documentation](https://github.com/google/sanitizers)

## Future Enhancements

- [ ] Add more seed corpus from real-world configs
- [ ] Integrate with OSS-Fuzz for 24/7 fuzzing
- [ ] Add coverage reports to gh-pages
- [ ] Create fuzzer for IMP execution paths
- [ ] Add dictionary files for better mutation
