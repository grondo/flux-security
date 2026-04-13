/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* Dual-mode fuzzing harness for flux_sign_unwrap().
 * Supports both AFL++ persistent mode and libFuzzer.
 *
 * Tests full signature verification path (slower than noverify variant).
 *
 * Note: For curve mechanism to work, you need a valid cert file.
 * For munge mechanism, munged must be running.
 *
 * Build for AFL++:     CC=afl-clang-fast ./configure --enable-fuzzing
 * Build for libFuzzer: CC=clang CFLAGS="-fsanitize=fuzzer" ./configure --enable-fuzzing
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "src/lib/context.h"
#include "src/lib/sign.h"

/* Config pattern for .toml files.
 * Use environment variable FUZZ_CONFIG_PATH or default paths.
 */
#ifndef FUZZ_CONFIG_PATH
#define FUZZ_CONFIG_PATH "src/fuzz/conf.d/*.toml"
#endif
#define FUZZ_CONFIG_PATH_ALT "conf.d/*.toml"

/* Limit input size to prevent memory exhaustion during fuzzing.
 * 1MB chosen as reasonable upper bound for signed payload parsing:
 * - Typical signed payloads are <1KB (job descriptions, configs)
 * - Base64 encoding inflates size by ~33%
 * - Prevents fuzzer from wasting cycles on unrealistically large inputs
 * - Prevents OOM when fuzzer generates huge test cases
 * Production code does not enforce this limit (handled by caller).
 */
#define MAX_INPUT_SIZE 1048576  /* 1MB */

/* Core fuzzing logic - shared between AFL++ and libFuzzer.
 * Tests: full signature verification including crypto operations.
 */
static int fuzz_sign_unwrap(flux_security_t *ctx,
                             const uint8_t *data,
                             size_t size)
{
    const void *payload;
    int payloadsz;
    int64_t userid;
    char *input;

    if (size > MAX_INPUT_SIZE)
        return 0;

    /* Need null-terminated string for flux_sign_unwrap() */
    input = malloc (size + 1);
    if (!input)
        return 0;
    memcpy (input, data, size);
    input[size] = '\0';

    /* Full verification (slower due to crypto, but tests complete path) */
    (void)flux_sign_unwrap (ctx,
                            input,
                            &payload,
                            &payloadsz,
                            &userid,
                            0);  /* flags=0: full verification */

    free (input);
    return 0;
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
/* ===================================================================
 * AFL++ PERSISTENT MODE
 * =================================================================== */

__AFL_FUZZ_INIT ();

int main (void)
{
    flux_security_t *ctx;
    unsigned char *buf;
    const char *config_path;
    int configured;

    /* Initialize AFL fork server FIRST, before any setup that might fail */
    __AFL_INIT ();
    buf = __AFL_FUZZ_TESTCASE_BUF;

    /* Suppress error messages for cleaner fuzzing (unless debugging) */
    if (!getenv ("FUZZ_DEBUG"))
        close (STDERR_FILENO);

    /* Create context - if this fails, we can't fuzz but AFL is already
     * initialized.
     */
    ctx = flux_security_create (0);
    if (!ctx)
        return 1;

    /* Try to configure - try multiple paths to find config.
     * Configuration is CRITICAL for proper fuzzing coverage.
     * Set FUZZ_CONFIG_PATH env var to override.
     */
    config_path = getenv ("FUZZ_CONFIG_PATH");
    configured = 0;

    if (config_path) {
        configured = (flux_security_configure (ctx, config_path) == 0);
    }
    else {
        /* Try default location (from project root) */
        if (flux_security_configure (ctx, FUZZ_CONFIG_PATH) == 0)
            configured = 1;
        /* Try alternate location (from src/fuzz dir) */
        else if (flux_security_configure (ctx, FUZZ_CONFIG_PATH_ALT) == 0)
            configured = 1;
    }

    /* If config failed, print error and exit - otherwise we waste CPU */
    if (!configured) {
        fprintf (stderr,
                 "FATAL: Could not load config. "
                 "Set FUZZ_CONFIG_PATH or create conf.d/sign.toml\n");
        fprintf (stderr, "Tried: %s and %s\n",
                 FUZZ_CONFIG_PATH,
                 FUZZ_CONFIG_PATH_ALT);
        flux_security_destroy (ctx);
        return 1;
    }

    while (__AFL_LOOP (10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        fuzz_sign_unwrap (ctx, buf, len);
    }

    flux_security_destroy (ctx);
    return 0;
}

#else
/* ===================================================================
 * LIBFUZZER MODE
 * =================================================================== */

static flux_security_t *global_ctx = NULL;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    const char *config_path;
    int configured = 0;

    (void)argc;  /* Unused */
    (void)argv;  /* Unused */

    /* Create context once at startup */
    global_ctx = flux_security_create (0);
    if (!global_ctx)
        return 1;

    /* Try to configure - try multiple paths to find config */
    config_path = getenv ("FUZZ_CONFIG_PATH");

    if (config_path) {
        configured = (flux_security_configure (global_ctx, config_path) == 0);
    }
    else {
        /* Try default location (from project root) */
        if (flux_security_configure (global_ctx, FUZZ_CONFIG_PATH) == 0)
            configured = 1;
        /* Try alternate location (from src/fuzz dir) */
        else if (flux_security_configure (global_ctx, FUZZ_CONFIG_PATH_ALT) == 0)
            configured = 1;
    }

    if (!configured) {
        fprintf (stderr,
                 "FATAL: Could not load config. "
                 "Set FUZZ_CONFIG_PATH or create conf.d/sign.toml\n");
        flux_security_destroy (global_ctx);
        global_ctx = NULL;
        return 1;
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!global_ctx)
        return 0;

    return fuzz_sign_unwrap (global_ctx, data, size);
}

#endif /* __AFL_FUZZ_TESTCASE_LEN */

/*
 * vi: ts=4 sw=4 expandtab
 */
