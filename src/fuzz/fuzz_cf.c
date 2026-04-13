/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* Dual-mode fuzzing harness for cf (configuration) interface.
 * Supports both AFL++ persistent mode and libFuzzer.
 *
 * This fuzzer targets the cf_t interface used by IMP for parsing TOML
 * configuration files. The cf layer sits on top of libtomlc99 and jansson,
 * providing parsing, validation, and type-safe access to configs.
 *
 * Bugs in this layer could allow privilege escalation since IMP configs
 * control security-critical settings (allowed-users, allowed-shells).
 *
 * Attack surfaces:
 * - cf_update(): TOML parsing and conversion to JSON
 * - cf_check(): Schema validation and type checking
 * - cf_get_in(): Nested table lookup
 * - cf_string(), cf_int64(), etc.: Type coercion and conversion
 * - cf_array_contains(): Array searching with pattern matching
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
#include <unistd.h>
#include <stdint.h>

#include "src/libutil/cf.h"

/* Limit input size to prevent memory exhaustion during fuzzing.
 * 1MB chosen as reasonable upper bound for TOML config parsing:
 * - libtomlc99 has known issues with large files causing hangs
 *   and integer overflow in byte offsets (see validate_toml_syntax)
 * - Typical flux-security configs are 10-100 lines (~1-10KB)
 * - Prevents fuzzer from wasting cycles on unrealistically large inputs
 * - Prevents OOM when fuzzer generates huge test cases
 * Production code validates input before parsing (MAX_LINES in tomltk.c).
 */
#define MAX_INPUT_SIZE 1048576  /* 1MB */

/* IMP-like configuration schema for realistic testing */
static const struct cf_option imp_opts[] = {
    {"allow-sudo",              CF_BOOL,     false},
    {"allow-unprivileged-exec", CF_BOOL,     false},
    {"pam-support",             CF_BOOL,     false},
    {"exec",                    CF_TABLE,    false},
    {"run",                     CF_TABLE,    false},
    {"sign",                    CF_TABLE,    false},
    CF_OPTIONS_TABLE_END,
};

static const struct cf_option exec_opts[] = {
    {"allowed-users",  CF_ARRAY,  false},
    {"allowed-shells", CF_ARRAY,  false},
    CF_OPTIONS_TABLE_END,
};

static const struct cf_option sign_opts[] = {
    {"max-ttl",        CF_INT64,  false},
    {"default-type",   CF_STRING, false},
    {"allowed-types",  CF_ARRAY,  false},
    CF_OPTIONS_TABLE_END,
};

/* Exercise all cf accessors on a table */
static void fuzz_exercise_table (const cf_t *cf)
{
    if (!cf || cf_typeof (cf) != CF_TABLE)
        return;

    /* Try accessing common config keys */
    const char *test_keys[] = {
        "allow-sudo", "allow-unprivileged-exec", "pam-support",
        "exec", "run", "sign", "allowed-users", "allowed-shells",
        "max-ttl", "default-type", "allowed-types",
        NULL
    };

    for (int i = 0; test_keys[i]; i++) {
        const cf_t *val = cf_get_in (cf, test_keys[i]);
        if (!val)
            continue;

        /* Try type-specific accessors */
        switch (cf_typeof (val)) {
            case CF_BOOL:
                (void)cf_bool (val);
                break;
            case CF_INT64:
                (void)cf_int64 (val);
                break;
            case CF_DOUBLE:
                (void)cf_double (val);
                break;
            case CF_STRING:
                (void)cf_string (val);
                break;
            case CF_ARRAY:
                (void)cf_array_size (val);
                /* Try array_contains with various patterns */
                (void)cf_array_contains (val, "test");
                (void)cf_array_contains (val, "/bin/sh");
                (void)cf_array_contains (val, "*");
                break;
            case CF_TABLE:
                /* Recurse into nested tables */
                fuzz_exercise_table (val);
                break;
            default:
                break;
        }
    }
}

/* Core fuzzing logic - shared between AFL++ and libFuzzer */
static int fuzz_cf(const uint8_t *data, size_t size)
{
    struct cf_error error;
    cf_t *cf;

    if (size > MAX_INPUT_SIZE)
        return 0;

    /* Create cf object (JSON table internally) */
    cf = cf_create ();
    if (!cf)
        return 0;

    /* Fuzz: Parse TOML and update cf object.
     * This exercises:
     * - TOML syntax parsing (libtomlc99)
     * - TOML-to-JSON conversion (tomltk_table_to_json)
     * - JSON deep merge (jansson)
     * - Error handling for malformed input
     */
    if (cf_update (cf, (char *)data, size, &error) == 0) {
        /* Successfully parsed - now exercise validation and accessors */

        /* Test schema validation with different strictness levels */
        (void)cf_check (cf, imp_opts, 0, &error);
        (void)cf_check (cf, imp_opts, CF_STRICT, &error);
        (void)cf_check (cf, imp_opts, CF_ANYTAB, &error);

        /* Validate nested tables if present */
        const cf_t *exec = cf_get_in (cf, "exec");
        if (exec) {
            (void)cf_check (exec, exec_opts, CF_STRICT, &error);
        }

        const cf_t *sign = cf_get_in (cf, "sign");
        if (sign) {
            (void)cf_check (sign, sign_opts, CF_STRICT, &error);
        }

        /* Exercise all accessor functions */
        fuzz_exercise_table (cf);

        /* Test cf_copy() */
        cf_t *copy = cf_copy (cf);
        if (copy) {
            fuzz_exercise_table (copy);
            cf_destroy (copy);
        }
    }
    /* If parsing failed, that's fine - error handling was exercised */

    cf_destroy (cf);
    return 0;
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
/* ===================================================================
 * AFL++ PERSISTENT MODE
 * =================================================================== */

__AFL_FUZZ_INIT ();

int main (void)
{
    unsigned char *buf;

    __AFL_INIT ();
    buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP (10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        fuzz_cf (buf, len);
    }

    return 0;
}

#else
/* ===================================================================
 * LIBFUZZER MODE
 * =================================================================== */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    return fuzz_cf (data, size);
}

#endif /* __AFL_FUZZ_TESTCASE_LEN */

/*
 * vi: ts=4 sw=4 expandtab
 */
