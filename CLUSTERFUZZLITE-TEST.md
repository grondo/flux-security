# ClusterFuzzLite Testing Guide

## Testing on Branch Push

The workflow is now configured to run on both PR events AND branch pushes, so you can test before opening a PR.

## Quick Test Steps

### 1. Create and Push Test Branch

```bash
# Make sure all ClusterFuzzLite files are in your branch
git status

# You should see:
# .clusterfuzzlite/
# .github/workflows/clusterfuzzlite-pr.yml
# .github/workflows/clusterfuzzlite-batch.yml

# Create a test branch
git checkout -b test-clusterfuzzlite

# Add all ClusterFuzzLite files
git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml
git add CLUSTERFUZZLITE-SETUP.md

# Commit
git commit -m "Add ClusterFuzzLite CI fuzzing integration"

# Push to your fork/repo
git push origin test-clusterfuzzlite
```

### 2. Watch GitHub Actions

1. Go to your repo on GitHub
2. Click the **"Actions"** tab
3. You should see a new workflow run: **"ClusterFuzzLite Fuzzing"**
4. Click on it to watch progress

### 3. Monitor Progress

**Timeline:**
- **0-3 min**: Workflow starts, checks out code
- **3-8 min**: Builds fuzzers with AddressSanitizer
- **8-18 min**: Runs fuzzing (10 minutes)
- **18-20 min**: Uploads artifacts, reports results

**Expected Steps:**
1. ✅ Build Fuzzers (address) - ~5 min
2. ✅ Run Fuzzers (address) - ~10 min
3. ✅ Report Results for Push Event - <1 min

### 4. Check Results

#### Success Case (Expected)

Look for the "Report Results for Push Event" step output:

```
==========================================
ClusterFuzzLite Fuzzing Results
==========================================
Event: push to test-clusterfuzzlite
Sanitizer: address
Status: success

✅ No crashes found during 10-minute fuzzing session

Fuzzers tested:
  - fuzz_sign_unwrap (signature verification parser)
  - fuzz_sign_unwrap_noverify (fast parser, no crypto)
  - fuzz_kv (KV format parser for privsep)
  - fuzz_cf (TOML configuration parser)

View full results in the workflow artifacts above
```

You'll also see a green checkmark (✅) next to the workflow.

#### Failure Case (Unexpected)

If crashes are found, you'll see:
- Red X (❌) on the workflow
- "Upload Crashes" step with artifacts
- Error message in "Report Results" step

**Action:** Download crash artifacts and investigate.

## What to Look For

### Build Step Should Show:

```
=== Installing dependencies ===
Reading package lists...
Building dependency tree...
...
libjansson-dev is already the newest version
libsodium-dev is already the newest version
...

=== Running configure ===
checking for gcc... clang
...
checking for AFL compiler... yes
...

=== Building project ===
make[1]: Entering directory '/src/flux-security'
...
CC       src/fuzz/fuzz_sign_unwrap.o
CCLD     fuzz_sign_unwrap
...

Build completed successfully!
Fuzzers built:
-rwxr-xr-x 1 root root 2.1M fuzz_sign_unwrap
-rwxr-xr-x 1 root root 2.0M fuzz_sign_unwrap_noverify
-rwxr-xr-x 1 root root 1.8M fuzz_kv
-rwxr-xr-x 1 root root 2.2M fuzz_cf

Corpus seeds:
  fuzz_sign_unwrap_seed_corpus: 1 files
  fuzz_sign_unwrap_noverify_seed_corpus: 1 files
  fuzz_kv_seed_corpus: 1 files
  fuzz_cf_seed_corpus: 1 files
```

### Fuzzing Step Should Show:

```
Running 4 fuzzers in parallel for 600 seconds total
  - fuzz_sign_unwrap
  - fuzz_sign_unwrap_noverify
  - fuzz_kv
  - fuzz_cf

[fuzzer progress bars and stats]

Summary:
  Total execs: ~50M-100M (depends on fuzzer speed)
  New paths: 10-50 (depends on corpus)
  Crashes: 0
  Hangs: 0
```

## Troubleshooting

### Workflow Doesn't Start

**Check 1:** Verify files are committed and pushed
```bash
git log --oneline -1  # Should show your commit
git ls-files .github/workflows/clusterfuzzlite-pr.yml  # Should be tracked
```

**Check 2:** Check if workflow is enabled
- Go to Actions tab
- Look for "ClusterFuzzLite Fuzzing" in left sidebar
- If disabled, enable it

**Check 3:** Check the paths trigger
The workflow only runs if you change:
- `src/**`
- `configure.ac`
- `.clusterfuzzlite/**`
- `.github/workflows/clusterfuzzlite-*.yml`

### Build Fails

**Error: "apt-get: command not found"**
- Not expected - workflow uses Ubuntu 22.04

**Error: "configure: error: --enable-fuzzing requires CC to be an AFL compiler"**
- Check that ClusterFuzzLite sets CC properly
- Review build.sh line where it runs configure

**Error: Missing dependencies**
```bash
# Update .clusterfuzzlite/build.sh
apt-get install -y <missing-dependency>
```

### Fuzzing Fails

**Error: "fuzz_sign_unwrap: error while loading shared libraries"**
- Check that configure uses --disable-shared
- Verify LDFLAGS in build.sh

**Error: "FUZZ_CONFIG_PATH not set"**
- Check that conf.d/sign.toml was copied
- Verify environment variable in harness

### Slow Fuzzing

**Expected speeds:**
- fuzz_sign_unwrap: ~20-50k exec/sec (slower, has crypto)
- fuzz_sign_unwrap_noverify: ~180k exec/sec
- fuzz_kv: ~200k exec/sec
- fuzz_cf: ~100-150k exec/sec

If much slower:
- Check sanitizer overhead (ASan adds ~2x slowdown)
- Verify persistent mode is working (AFL_LOOP)
- Check for excessive logging

## After Successful Test

Once the workflow passes:

### Option 1: Merge into your main branch
```bash
# If you're happy with the test
git checkout fuzzer
git merge test-clusterfuzzlite
git push origin fuzzer
```

### Option 2: Open a PR
```bash
# Create PR from test-clusterfuzzlite to fuzzer
# Workflow will run again and comment on PR
```

### Option 3: Clean up test branch
```bash
# If you want to test again
git branch -D test-clusterfuzzlite
git push origin --delete test-clusterfuzzlite
```

## Expected Files After Push

In your GitHub repo:
- `.github/workflows/clusterfuzzlite-pr.yml` ✅
- `.github/workflows/clusterfuzzlite-batch.yml` ✅
- `.clusterfuzzlite/build.sh` ✅
- `.clusterfuzzlite/project.yaml` ✅
- `.clusterfuzzlite/README.md` ✅
- `.clusterfuzzlite/INTEGRATION-GUIDE.md` ✅
- `.clusterfuzzlite/test-build-local.sh` ✅

In GitHub Actions:
- New workflow: "ClusterFuzzLite Fuzzing" ✅
- Triggered on push to test-clusterfuzzlite ✅
- Status should be green ✅

## Next Steps

After successful test:

1. ✅ Verify build works (step 1)
2. ✅ Verify fuzzing runs (step 2)
3. ✅ Verify results reported (step 3)
4. Create PR to merge into main branch
5. Watch for PR comment (this time it will comment!)
6. Wait for first nightly batch run (2 AM UTC)

## Need Help?

- **Build issues**: Check `.clusterfuzzlite/build.sh`
- **Fuzzing issues**: Check harness sources in `src/fuzz/`
- **Workflow issues**: Check `.github/workflows/clusterfuzzlite-pr.yml`
- **General questions**: Read `.clusterfuzzlite/INTEGRATION-GUIDE.md`

## Summary

```bash
# Quick test command sequence:
git checkout -b test-clusterfuzzlite
git add .clusterfuzzlite/ .github/workflows/clusterfuzzlite*.yml CLUSTERFUZZLITE*.md
git commit -m "Add ClusterFuzzLite CI fuzzing"
git push origin test-clusterfuzzlite

# Then watch: https://github.com/YOUR-USERNAME/flux-security/actions
```

Expected result: ✅ Green workflow after ~20 minutes
