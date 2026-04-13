# ClusterFuzzLite Integration - Setup Complete ✅

## What Was Added

ClusterFuzzLite integration for automated continuous fuzzing in CI/CD.

### Files Created

```
.clusterfuzzlite/
├── build.sh                  # Builds fuzzers for ClusterFuzzLite
├── project.yaml              # Project configuration
├── README.md                 # Complete documentation
├── INTEGRATION-GUIDE.md      # Integration and troubleshooting guide
└── test-build-local.sh       # Local build test script

.github/workflows/
├── clusterfuzzlite-pr.yml       # PR fuzzing (10 min per PR)
└── clusterfuzzlite-batch.yml    # Batch fuzzing (6 hours nightly)
```

## Features

### 🔄 Automated PR Fuzzing
- **Trigger**: Every pull request
- **Duration**: 10 minutes across all fuzzers
- **Sanitizer**: AddressSanitizer
- **Output**: PR comment with results (✅/❌)
- **Action**: Fails PR if crashes found

### 🌙 Nightly Batch Fuzzing
- **Trigger**: 2 AM UTC daily (or manual)
- **Duration**: 6 hours per sanitizer
- **Sanitizers**: AddressSanitizer, UndefinedBehaviorSanitizer, MemorySanitizer
- **Output**: GitHub issue if crashes found
- **Corpus**: Stored in `clusterfuzzlite-corpus` branch

### 🎯 Fuzzers Integrated
1. `fuzz_sign_unwrap` - Full signature verification
2. `fuzz_sign_unwrap_noverify` - Fast parser (no crypto)
3. `fuzz_kv` - KV format (privsep communication)
4. `fuzz_cf` - TOML configuration parser

## Testing Before Merge

### Option 1: Quick Local Test (Recommended)

```bash
# Test the build in Docker (5-10 minutes)
./.clusterfuzzlite/test-build-local.sh
```

This will:
- Build fuzzers in Docker container
- Verify all 4 fuzzers are built
- Check seed corpus creation
- Validate config files
- Run quick fuzzer test

**Expected output:**
```
✅ BUILD TEST PASSED

All fuzzers built successfully!
Build artifacts:
-rwxr-xr-x 1 user user 2.1M fuzz_sign_unwrap
-rwxr-xr-x 1 user user 2.0M fuzz_sign_unwrap_noverify
-rwxr-xr-x 1 user user 1.8M fuzz_kv
-rwxr-xr-x 1 user user 2.2M fuzz_cf
```

### Option 2: Test on GitHub

```bash
# Add and commit ClusterFuzzLite files
git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml
git commit -m "Add ClusterFuzzLite CI fuzzing integration"

# Push to your branch
git push origin fuzzer

# Open PR and watch for ClusterFuzzLite comment within 15 minutes
```

## How It Works

### PR Workflow

```
Developer opens PR
    ↓
GitHub Actions triggered
    ↓
Build fuzzers with ASan (5 min)
    ↓
Run 10 min of fuzzing
    ↓
Post comment on PR:
  - ✅ "No crashes found" → PR passes
  - ❌ "Crashes detected" → PR fails
    ↓
Upload crash artifacts if any
```

### Batch Workflow

```
2 AM UTC (nightly) or manual trigger
    ↓
Build fuzzers with ASan, UBSan, MSan
    ↓
Run 6 hours of fuzzing per sanitizer
    ↓
Store corpus in clusterfuzzlite-corpus branch
    ↓
If crashes found:
  - Create GitHub issue
  - Upload artifacts
  - Tag: fuzzing, bug, security
```

## Benefits Over Manual Fuzzing

| Feature | Manual AFL++ | ClusterFuzzLite CI |
|---------|-------------|-------------------|
| **Automation** | Manual | Automatic |
| **PR Coverage** | None | Every PR |
| **Frequency** | On demand | Daily + PRs |
| **Sanitizers** | ASan only | ASan + UBSan + MSan |
| **Issue Tracking** | Manual | Auto GitHub issues |
| **Corpus Management** | Local | Git branch |
| **Developer Overhead** | High | Zero |

**ClusterFuzzLite complements, doesn't replace, manual AFL++ fuzzing.**

## Expected Results

### First PR After Merge

**Expected timeline:**
- 0-3 min: Workflow starts, builds fuzzers
- 3-13 min: Fuzzing runs (10 min)
- 13-15 min: Results posted as PR comment

**Expected comment:**
```
### ClusterFuzzLite PR Fuzzing Results (address)

✅ No crashes found during 10-minute fuzzing session.

Fuzzers tested:
- fuzz_sign_unwrap - Signature verification parser
- fuzz_sign_unwrap_noverify - Fast parser (no crypto)
- fuzz_kv - KV format parser (privsep)
- fuzz_cf - TOML configuration parser
```

### First Nightly Run

**Expected timeline:**
- Day 1, 2 AM UTC: First batch run starts
- 8 AM UTC: Completes (6 hours)
- Creates `clusterfuzzlite-corpus` branch with discovered inputs

**Expected result:**
- No issues created (existing bugs already fixed)
- Corpus grows as new code paths discovered
- Coverage reports generated

## Troubleshooting

### Build Fails in CI

**Check**: Dependencies
```bash
# Update .clusterfuzzlite/build.sh if dependencies changed
apt-get install -y <new-dependency>
```

**Check**: Configure flags
```bash
# Verify --enable-fuzzing works locally
CC=clang ./configure --enable-fuzzing
make
```

### No PR Comment Appears

**Check**: Workflow permissions
- Go to repo Settings → Actions → General
- Ensure "Read and write permissions" is enabled

**Check**: Workflow triggered
- Go to Actions tab
- Look for "ClusterFuzzLite PR Fuzzing" workflow
- Check logs for errors

### Fuzzer Crashes Immediately

**Check**: Config files
```bash
# Verify conf.d/sign.toml is copied
# Check .clusterfuzzlite/build.sh lines 85-95
```

**Check**: Environment variables
```bash
# Some fuzzers need FUZZ_CONFIG_PATH
export FUZZ_CONFIG_PATH=/out/conf.d/*.toml
```

## Customization

### Adjust Fuzzing Time

**Shorter PR fuzzing** (5 min instead of 10):
```yaml
# Edit .github/workflows/clusterfuzzlite-pr.yml
fuzz-seconds: 300  # Change from 600
```

**Longer batch fuzzing** (12 hours instead of 6):
```yaml
# Edit .github/workflows/clusterfuzzlite-batch.yml
fuzz-seconds: 43200  # Change from 21600
```

### Add More Sanitizers to PR

```yaml
# Edit .github/workflows/clusterfuzzlite-pr.yml
matrix:
  sanitizer: [address, undefined]  # Add UBSan
```

### Change Batch Schedule

```yaml
# Edit .github/workflows/clusterfuzzlite-batch.yml
on:
  schedule:
    - cron: '0 6 * * *'  # Change to 6 AM UTC instead of 2 AM
```

## Next Steps After Merge

### Immediate (Within 1 Week)
1. ✅ Monitor first PR fuzzing run (verify it works)
2. ✅ Check first nightly batch run (verify corpus created)
3. ✅ Review any issues created (should be none)
4. ✅ Verify `clusterfuzzlite-corpus` branch exists

### Short Term (Within 1 Month)
- Analyze corpus growth rate
- Tune fuzzing time if needed
- Add better seed corpus from real configs
- Consider adding dictionary files

### Long Term (Future)
- Apply for OSS-Fuzz integration (24/7 Google-hosted fuzzing)
- Set up coverage reports on gh-pages
- Create structure-aware mutators
- Add fuzzer for full IMP execution paths

## Documentation

- **Quick Reference**: `.clusterfuzzlite/README.md`
- **Integration Guide**: `.clusterfuzzlite/INTEGRATION-GUIDE.md`
- **ClusterFuzzLite Docs**: https://google.github.io/clusterfuzzlite/
- **Example Projects**: https://github.com/google/oss-fuzz/tree/master/projects

## Support

If you encounter issues:
1. Check `.clusterfuzzlite/INTEGRATION-GUIDE.md` troubleshooting section
2. Review workflow logs in GitHub Actions tab
3. Test build locally with `.clusterfuzzlite/test-build-local.sh`
4. Check ClusterFuzzLite documentation
5. File issue in repo with `fuzzing` label

---

## Summary

✅ **Automated fuzzing** on every PR (10 min)  
✅ **Nightly fuzzing** with multiple sanitizers (6 hours)  
✅ **Auto issue creation** when crashes found  
✅ **Corpus management** in Git  
✅ **Zero maintenance** overhead  
✅ **Complements existing** AFL++ setup  

**Ready to merge!** 🚀

Test locally first with: `./.clusterfuzzlite/test-build-local.sh`
