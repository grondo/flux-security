# ClusterFuzzLite Integration Guide

## What Was Added

### Directory Structure
```
.clusterfuzzlite/
├── build.sh              # Build script for fuzzing harnesses
├── project.yaml          # ClusterFuzzLite configuration
├── README.md             # Complete documentation
└── INTEGRATION-GUIDE.md  # This file

.github/workflows/
├── clusterfuzzlite-pr.yml     # PR fuzzing (10 min per PR)
└── clusterfuzzlite-batch.yml  # Nightly fuzzing (6 hours)
```

## How It Works

### On Every Pull Request
1. When you open/update a PR, GitHub Actions triggers
2. ClusterFuzzLite builds all 4 fuzzers with AddressSanitizer
3. Runs 10 minutes of fuzzing across all harnesses
4. Comments on PR with results:
   - ✅ Green check if no crashes found
   - ❌ Red X if crashes found (PR fails)
5. Uploads crash artifacts if any found

### Nightly (2 AM UTC)
1. Builds fuzzers with 3 sanitizers (Address, Undefined, Memory)
2. Runs 6 hours per sanitizer (18 hours total)
3. Stores corpus in `clusterfuzzlite-corpus` branch
4. If crashes found:
   - Creates GitHub issue automatically
   - Uploads crash artifacts
   - Tags issue with `fuzzing`, `bug`, `security`

## Testing the Setup

### Test Locally (Before Pushing)

```bash
# Verify build script works
cd flux-security
docker run --rm -v $(pwd):/src -w /src ubuntu:22.04 bash -c '
  export OUT=/tmp/out
  export SRC=/src
  export CC=clang
  export CFLAGS="-fsanitize=address -fsanitize=fuzzer-no-link"
  export CXXFLAGS="-fsanitize=address -fsanitize=fuzzer-no-link"
  export LIB_FUZZING_ENGINE=""
  bash .clusterfuzzlite/build.sh
'

# Check that fuzzers were built
ls -lh /tmp/out/fuzz_*

# Manually test a fuzzer
echo "test" | /tmp/out/fuzz_kv
```

### Test on GitHub

#### Option 1: Create Test PR
```bash
# Create a test branch
git checkout -b test-clusterfuzzlite
git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml
git commit -m "Add ClusterFuzzLite integration"
git push origin test-clusterfuzzlite

# Open PR on GitHub
# Watch for comment from ClusterFuzzLite bot
```

#### Option 2: Manual Workflow Trigger
```bash
# Push to main branch
git checkout fuzzer
git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml
git commit -m "Add ClusterFuzzLite CI fuzzing"
git push origin fuzzer

# Go to Actions tab on GitHub
# Click "ClusterFuzzLite Batch Fuzzing"
# Click "Run workflow" button
# Select branch and click "Run workflow"
```

## Expected Behavior

### First Run
- **Build time**: ~5-10 minutes (installs dependencies, builds project)
- **Fuzzing time**: 10 minutes (PR) or 6 hours (batch)
- **Output**: PR comment or GitHub issue

### Subsequent Runs
- **Faster builds**: Docker layer caching helps
- **Better coverage**: Corpus grows over time
- **Fewer false positives**: Corpus minimization removes duplicates

## Troubleshooting

### Build Fails

**Check 1: Dependencies**
```bash
# Update .clusterfuzzlite/build.sh apt-get install list
# If new dependencies were added to flux-security
```

**Check 2: Configure Flags**
```bash
# Verify --enable-fuzzing works
./configure --enable-fuzzing
make
```

**Check 3: Compiler**
```bash
# ClusterFuzzLite sets CC to instrumented compiler
# Make sure configure respects $CC environment variable
```

### Fuzzing Fails

**Check 1: Config Files**
```bash
# Fuzzers need conf.d/sign.toml
# Verify it's copied to $OUT in build.sh
ls -la $OUT/conf.d/sign.toml
```

**Check 2: Environment**
```bash
# Some fuzzers need FUZZ_CONFIG_PATH
# Set in build.sh: export FUZZ_CONFIG_PATH=$OUT/conf.d/*.toml
```

**Check 3: Corpus**
```bash
# Verify seed corpus was created
ls -la $OUT/*_seed_corpus/
```

### No PR Comment

**Check 1: Workflow Permissions**
```yaml
# In clusterfuzzlite-pr.yml, verify:
permissions: read-all
# GitHub token needs write:issues for comments
```

**Check 2: Workflow Triggers**
```yaml
# Verify workflow runs on PR:
on:
  pull_request:
    paths:
      - 'src/**'
```

## Customization

### Adjust Fuzzing Time

**PR Fuzzing** (currently 10 minutes):
```yaml
# In .github/workflows/clusterfuzzlite-pr.yml
fuzz-seconds: 600  # Change to 300 (5 min) or 1200 (20 min)
```

**Batch Fuzzing** (currently 6 hours):
```yaml
# In .github/workflows/clusterfuzzlite-batch.yml
fuzz-seconds: 21600  # Change to 3600 (1 hour) or 43200 (12 hours)
```

### Add New Fuzzer

1. Create fuzzer in `src/fuzz/fuzz_newharness.c`
2. Add to `src/fuzz/Makefile.am`
3. Update `.clusterfuzzlite/build.sh`:
```bash
# Copy new fuzzer
cp src/fuzz/fuzz_newharness $OUT/

# Create seed corpus
mkdir -p $OUT/fuzz_newharness_seed_corpus
echo "seed" > $OUT/fuzz_newharness_seed_corpus/minimal.txt
```
4. Update `.clusterfuzzlite/project.yaml`:
```yaml
main_fuzzers:
  - fuzz_sign_unwrap
  - fuzz_sign_unwrap_noverify
  - fuzz_kv
  - fuzz_cf
  - fuzz_newharness  # Add here
```

### Change Sanitizers

**PR Fuzzing** (currently ASan only):
```yaml
# In .github/workflows/clusterfuzzlite-pr.yml
matrix:
  sanitizer: [address, undefined]  # Add undefined behavior sanitizer
```

**Batch Fuzzing** (currently ASan, UBSan, MSan):
```yaml
# In .github/workflows/clusterfuzzlite-batch.yml
matrix:
  sanitizer: [address]  # Reduce to just ASan if needed
```

### Disable for Certain PRs

Add to `.github/workflows/clusterfuzzlite-pr.yml`:
```yaml
on:
  pull_request:
    paths-ignore:
      - 'docs/**'
      - '**.md'
      - 'README*'
```

## Integration with Existing Fuzzing

ClusterFuzzLite complements your existing AFL++ setup:

| Feature | AFL++ (Local) | ClusterFuzzLite (CI) |
|---------|---------------|----------------------|
| **Trigger** | Manual | Automatic (PR/nightly) |
| **Duration** | Hours to days | 10 min (PR) / 6 hours (batch) |
| **Coverage** | Comprehensive | Quick smoke test |
| **Sanitizers** | ASan (configurable) | ASan, UBSan, MSan |
| **Corpus** | Local findings/ | GitHub corpus branch |
| **Reporting** | Manual analysis | Auto issue creation |
| **Use Case** | Deep testing, dev | Regression catching, CI |

**Workflow:**
1. **Local development**: Use AFL++ for deep testing (hours/days)
2. **PR submission**: ClusterFuzzLite catches regressions (10 min)
3. **Nightly**: ClusterFuzzLite finds new bugs (6 hours)
4. **Bug found**: Reproduce locally with AFL++, fix, add test

## Next Steps

### After Merging This PR

1. **Watch First Run**: Monitor the first PR/batch fuzzing run
2. **Verify Corpus**: Check that `clusterfuzzlite-corpus` branch is created
3. **Review Coverage**: Look at coverage reports (optional, requires setup)
4. **Tune Timing**: Adjust fuzz-seconds based on experience

### Future Enhancements

- **OSS-Fuzz**: Apply for continuous fuzzing (24/7 on Google infra)
- **Coverage Reports**: Set up coverage dashboard on gh-pages
- **Dictionary Files**: Add fuzzing dictionaries for smarter mutations
- **Custom Mutators**: Implement structure-aware mutators

## Resources

- **ClusterFuzzLite Docs**: https://google.github.io/clusterfuzzlite/
- **Example Projects**: https://github.com/google/oss-fuzz/tree/master/projects
- **libFuzzer Guide**: https://llvm.org/docs/LibFuzzer.html
- **Sanitizer Docs**: https://github.com/google/sanitizers/wiki

## Support

If issues arise:
1. Check workflow logs in GitHub Actions
2. Review `.clusterfuzzlite/README.md` for detailed docs
3. Check ClusterFuzzLite GitHub issues
4. File issue in flux-security repo with `fuzzing` label
