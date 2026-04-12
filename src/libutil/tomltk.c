/************************************************************\
 * Copyright 2017 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <jansson.h>

#include "src/libtomlc99/toml.h"
#include "timestamp.h"
#include "tomltk.h"

static int table_to_json (toml_table_t *tab, json_t **op);

static void errprintf (struct tomltk_error *error,
                       const char *filename, int lineno,
                       const char *fmt, ...)
{
    va_list ap;
    int saved_errno = errno;

    if (error) {
        memset (error, 0, sizeof (*error));
        va_start (ap, fmt);
        (void)vsnprintf (error->errbuf, sizeof (error->errbuf), fmt, ap);
        va_end (ap);
        if (filename)
            strncpy (error->filename, filename, PATH_MAX);
        error->lineno = lineno;
    }
    errno = saved_errno;
}

/* Quick validation to reject inputs that cause libtomlc99 to hang.
 * This is a fast pre-filter before calling the full parser.
 *
 * CONTEXT: libtomlc99 (https://github.com/cktan/tomlc99) is no longer
 * actively maintained. AFL++ fuzzing discovered multiple inputs that cause
 * the parser to enter infinite loops (21 unique hang inputs). Rather than
 * fork and maintain libtomlc99 ourselves, we add pre-validation to reject
 * problematic patterns before they reach the parser. This is a temporary
 * mitigation until libtomlc99 can be replaced with a maintained alternative.
 *
 * The limits below are conservative values based on fuzzing results:
 * - Inputs exceeding these limits triggered parser hangs
 * - Legitimate flux-security configs are well below these thresholds
 * - Values chosen to fail fast (~0.5ms) rather than hang indefinitely (5+ sec)
 */
static int validate_toml_syntax (const char *conf, int len,
                                 struct tomltk_error *error)
{
    int bracket_depth = 0;
    int max_depth = 0;
    int square_count = 0;
    char in_string = 0;  // 0 = not in string, '"' or '\'' = quote type that opened
    int in_ml_double = 0;  // In """ multi-line string
    int in_ml_single = 0;  // In ''' multi-line string
    int escape_next = 0;
    int in_array = 0;  // Track if we're inside an array value

    /* MAX_NESTING: Limit bracket nesting depth.
     * Fuzzing found that deeply nested arrays (e.g., [[[[[[...]]]]]])
     * cause libtomlc99 to hang in recursive descent parsing. Set to 32
     * based on fuzzing observations: legitimate configs use ≤3 levels,
     * hangs occurred at 50+ levels. Value of 32 provides safety margin
     * while preventing pathological inputs.
     */
    const int MAX_NESTING = 32;

    /* MAX_LINES: Limit total input lines.
     * Fuzzing found that extremely large inputs with certain patterns
     * (embedded NULs, mismatched quotes, malformed arrays) cause the
     * parser to hang in string processing loops. Set to 10,000 lines
     * based on fuzzing observations: typical flux-security configs are
     * 10-100 lines, hangs occurred with generated inputs >50K lines.
     * Value of 10K provides generous headroom while preventing DoS via
     * parser resource exhaustion.
     */
    const int MAX_LINES = 10000;

    int line_count = 0;
    int i;
    int skip = 0;  // Track characters to skip (for multi-line delimiters)

    for (i = 0; i < len; i++) {
        char c = conf[i];

        // Skip characters (used when consuming multi-char sequences)
        if (skip > 0) {
            skip--;
            continue;
        }

        // Count newlines to detect excessive input
        if (c == '\n') {
            line_count++;
            if (line_count > MAX_LINES) {
                errprintf (error, NULL, -1,
                          "Input too large (>%d lines)", MAX_LINES);
                return -1;
            }
        }

        /* Reject adjacent triple-quote sequences (6 consecutive quotes).
         * Patterns like '''''' or """""" create ambiguous or zero-length
         * multi-line strings that cause libtomlc99 to enter infinite loops.
         * While technically valid TOML in some interpretations, these serve
         * no legitimate purpose and consistently trigger parser hangs.
         */
        if (i + 5 < len) {
            if ((conf[i] == '"' && conf[i+1] == '"' && conf[i+2] == '"' &&
                 conf[i+3] == '"' && conf[i+4] == '"' && conf[i+5] == '"') ||
                (conf[i] == '\'' && conf[i+1] == '\'' && conf[i+2] == '\'' &&
                 conf[i+3] == '\'' && conf[i+4] == '\'' && conf[i+5] == '\'')) {
                errprintf (error, NULL, -1,
                          "Adjacent triple-quote sequences not allowed");
                return -1;
            }
        }

        // Check for multi-line string delimiters
        if (i + 2 < len && !escape_next) {
            if (conf[i] == '"' && conf[i+1] == '"' && conf[i+2] == '"') {
                if (in_ml_double) {
                    // Closing multi-line double-quote string
                    in_ml_double = 0;
                    skip = 2;  // Skip next 2 quotes
                    continue;
                } else if (!in_ml_single && !in_string) {
                    // Opening multi-line double-quote string
                    in_ml_double = 1;
                    skip = 2;  // Skip next 2 quotes
                    continue;
                }
            }
            else if (conf[i] == '\'' && conf[i+1] == '\'' && conf[i+2] == '\'') {
                if (in_ml_single) {
                    // Closing multi-line single-quote string
                    in_ml_single = 0;
                    skip = 2;  // Skip next 2 quotes
                    continue;
                } else if (!in_ml_double && !in_string) {
                    // Opening multi-line single-quote string
                    in_ml_single = 1;
                    skip = 2;  // Skip next 2 quotes
                    continue;
                }
            }
        }

        // Inside multi-line strings, only look for the closing delimiter
        if (in_ml_double || in_ml_single) {
            continue;
        }

        /* Track regular string state (single " or ').
         * TOML requires matching quote types: strings that start with "
         * must end with ", and strings that start with ' must end with '.
         * Mismatched quotes like 'string"] cause parser hangs.
         */
        if (!escape_next && (c == '"' || c == '\'')) {
            if (in_string == 0) {
                // Opening a new string
                in_string = c;
            } else if (in_string == c) {
                // Closing string with matching quote type
                in_string = 0;
            }
            // Else: wrong quote type, ignore it (it's part of the string content)
            continue;
        }
        if (in_string) {
            escape_next = (!escape_next && c == '\\');
            continue;
        }
        escape_next = 0;

        // Handle comments - skip to end of line
        if (c == '#') {
            // Comments inside array values cause hangs
            if (in_array) {
                errprintf (error, NULL, -1,
                          "Comment character inside array value");
                return -1;
            }
            // Skip rest of line by counting chars until newline
            int j;
            for (j = i + 1; j < len && conf[j] != '\n'; j++)
                ;  // Just counting
            skip = j - i - 1;  // Skip all chars until (but not including) newline
            continue;
        }

        // Count brackets outside strings
        if (c == '[') {
            square_count++;
            bracket_depth++;
            if (bracket_depth > max_depth)
                max_depth = bracket_depth;

            // Detect array values (not table headers)
            if (bracket_depth == 1 && !in_array) {
                // Check if this is at start of line (ignoring whitespace)
                // If so, it's likely a table header [section], not an array value
                int j = i - 1;
                int is_table = 1;
                while (j >= 0 && conf[j] != '\n') {
                    if (conf[j] != ' ' && conf[j] != '\t') {
                        is_table = 0;
                        break;
                    }
                    j--;
                }
                if (!is_table && i > 0) {
                    in_array = 1;
                }
            }
            else if (bracket_depth > 1 || in_array) {
                in_array = 1;
            }

            // Reject excessive nesting (catches deeply nested arrays)
            if (bracket_depth > MAX_NESTING) {
                errprintf (error, NULL, -1,
                          "Excessive bracket nesting depth (%d)",
                          bracket_depth);
                return -1;
            }
        }
        else if (c == ']') {
            square_count--;
            bracket_depth--;

            // When we close all brackets, we're out of any array
            if (bracket_depth == 0) {
                in_array = 0;
            }

            // Reject if more closing than opening brackets
            if (square_count < 0) {
                errprintf (error, NULL, -1, "Unbalanced brackets");
                return -1;
            }
        }
    }

    // Reject if brackets don't balance
    if (square_count != 0) {
        errprintf (error, NULL, -1, "Unbalanced brackets");
        return -1;
    }

    // Reject if multi-line strings aren't closed
    if (in_ml_double) {
        errprintf (error, NULL, -1, "Unterminated multi-line string (\"\"\")");
        return -1;
    }
    if (in_ml_single) {
        errprintf (error, NULL, -1, "Unterminated multi-line string (''')");
        return -1;
    }

    return 0;
}

/* Given an error message response from toml_parse(), parse the
 * error message into line number and message, e.g.
 *   "line 42: bad key"
 * is parsed to:
 *   error->lineno=42, error->errbuf="bad key"
 */
static void errfromtoml (struct tomltk_error *error,
                         const char *filename, char *errstr)
{
    if (error) {
        char *msg = errstr;
        int lineno = -1;
        if (!strncmp (errstr, "line ", 5)) {
            lineno = strtoul (errstr + 5, &msg, 10);
            if (!strncmp (msg, ": ", 2))
                msg += 2;
        }
        return errprintf (error, filename, lineno, "%s", msg);
    }
}

/* Convert from TOML timestamp to struct tm (POSIX broken out time).
 */
static int tstotm (toml_timestamp_t *ts, struct tm *tm)
{
    if (!ts || !tm || !ts->year || !ts->month || !ts->day
                   || !ts->hour  || !ts->minute || !ts->second)
        return -1;
    memset (tm, 0, sizeof (*tm));
    tm->tm_year = *ts->year - 1900;
    tm->tm_mon = *ts->month - 1;
    tm->tm_mday = *ts->day;
    tm->tm_hour = *ts->hour;
    tm->tm_min = *ts->minute;
    tm->tm_sec = *ts->second;
    return 0;
}

int tomltk_ts_to_epoch (toml_timestamp_t *ts, time_t *tp)
{
    struct tm tm;
    time_t t;

    if (!ts || tstotm (ts, &tm) < 0 || (t = timegm (&tm)) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (tp)
        *tp = t;
    return 0;
}

int tomltk_json_to_epoch (const json_t *obj, time_t *tp)
{
    const char *s;

    /* N.B. 'O' specifier not used, hence obj is not in danger
     * of being modified by json_unpack.
     */
    if (!obj || json_unpack ((json_t *)obj, "{s:s}", "iso-8601-ts", &s) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (timestamp_fromstr (s, tp) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

json_t *tomltk_epoch_to_json (time_t t)
{
    char timebuf[80];
    json_t *obj;

    if (timestamp_tostr (t, timebuf, sizeof (timebuf)) < 0) {
        errno = EINVAL;
        return NULL;
    }
    if (!(obj = json_pack ("{s:s}", "iso-8601-ts", timebuf))) {
        errno = EINVAL;
        return NULL;
    }
    return obj;
}

/* Convert raw TOML value from toml_raw_in() or toml_raw_at() to JSON.
 */
static int value_to_json (const char *raw, json_t **op)
{
    char *s = NULL;
    int b;
    int64_t i;
    double d;
    toml_timestamp_t ts;
    json_t *obj;

    if (toml_rtos (raw, &s) == 0) {
        obj = json_string (s);
        free (s);
        if (!obj)
            goto nomem;
    }
    else if (toml_rtob (raw, &b) == 0) {
        if (!(obj = b ? json_true () : json_false ()))
            goto nomem;
    }
    else if (toml_rtoi (raw, &i) == 0) {
        if (!(obj = json_integer (i)))
            goto nomem;
    }
    else if (toml_rtod (raw, &d) == 0) {
        if (!(obj = json_real (d)))
            goto nomem;
    }
    else if (toml_rtots (raw, &ts) == 0) {
        time_t t;
        if (tomltk_ts_to_epoch (&ts, &t) < 0
                || !(obj = tomltk_epoch_to_json (t)))
            goto error;
    }
    else {
        errno = EINVAL;
        goto error;
    }
    *op = obj;
    return 0;
nomem:
    errno = ENOMEM;
error:
    return -1;
}

/* Convert TOML array to JSON.
 */
static int array_to_json (toml_array_t *arr, json_t **op)
{
    int i;
    int saved_errno;
    json_t *obj;

    if (!(obj = json_array ()))
        goto nomem;
    for (i = 0; ; i++) {
        const char *raw;
        json_t *val;
        toml_table_t *tab;
        toml_array_t *subarr;

        if ((raw = toml_raw_at (arr, i))) {
            if (value_to_json (raw, &val) < 0)
                goto error;
        }
        else if ((tab = toml_table_at (arr, i))) {
            if (table_to_json (tab, &val) < 0)
                goto error;
        }
        else if ((subarr = toml_array_at (arr, i))) {
            if (array_to_json (subarr, &val) < 0)
                goto error;
        }
        else
            break;
        if (json_array_append_new (obj, val) < 0) {
            goto nomem;
        }
    }
    *op = obj;
    return 0;
nomem:
    errno = ENOMEM;
error:
    saved_errno = errno;
    json_decref (obj);
    errno = saved_errno;
    return -1;
}

/* Convert TOML table to JSON.
 */
static int table_to_json (toml_table_t *tab, json_t **op)
{
    int i;
    int saved_errno;
    json_t *obj;

    if (!(obj = json_object ()))
        goto nomem;
    for (i = 0; ; i++) {
        const char *key;
        const char *raw;
        toml_table_t *subtab;
        toml_array_t *arr;
        json_t *val = NULL;

        if (!(key = toml_key_in (tab, i)))
            break;
        if ((raw = toml_raw_in (tab, key))) {
            if (value_to_json (raw, &val) < 0)
                goto error;
        }
        else if ((subtab = toml_table_in (tab, key))) {
            if (table_to_json (subtab, &val) < 0)
                goto error;
        }
        else if ((arr = toml_array_in (tab, key))) {
            if (array_to_json (arr, &val) < 0)
                goto error;
        }
        if (json_object_set_new (obj, key, val) < 0) {
            goto nomem;
        }
    }
    *op = obj;
    return 0;
nomem:
    errno = ENOMEM;
error:
    saved_errno = errno;
    json_decref (obj);
    errno = saved_errno;
    return -1;
}

json_t *tomltk_table_to_json (toml_table_t *tab)
{
    json_t *obj;

    if (!tab) {
        errno = EINVAL;
        return NULL;
    }
    if (table_to_json (tab, &obj) < 0)
        return NULL;
    return obj;
}

toml_table_t *tomltk_parse (const char *conf, int len,
                            struct tomltk_error *error)
{
    char errbuf[200];
    char *cpy;
    toml_table_t *tab;

    if (len < 0 || (!conf && len != 0)) {
        errprintf (error, NULL, -1, "invalid argument");
        errno = EINVAL;
        return NULL;
    }
    if (len > 0 && memchr (conf, '\0', len) != NULL) {
        errprintf (error, NULL, -1, "Config contains embedded NUL byte");
        errno = EINVAL;
        return NULL;
    }
    if (len > 0 && validate_toml_syntax (conf, len, error) < 0) {
        errno = EINVAL;
        return NULL;
    }
    if (!(cpy = calloc (1, len + 1))) {
        errprintf (error, NULL, -1, "out of memory");
        errno = ENOMEM;
        return NULL;
    }
    memcpy (cpy, conf, len);
    tab = toml_parse (cpy, errbuf, sizeof (errbuf));
    free (cpy);
    if (!tab) {
        errfromtoml (error, NULL, errbuf);
        errno = EINVAL;
        return NULL;
    }
    return tab;
}

toml_table_t *tomltk_parse_file (const char *filename,
                                 struct tomltk_error *error)
{
    char errbuf[200];
    FILE *fp;
    toml_table_t *tab;

    if (!filename) {
        errprintf (error, NULL, -1, "invalid argument");
        errno = EINVAL;
        return NULL;
    }
    if (!(fp = fopen (filename, "r"))) {
        errprintf (error, filename, -1, "%s", strerror (errno));
        return NULL;
    }
    // N.B. toml_parse_file() doesn't give us any way to distinguish parse
    // error from read error
    tab = toml_parse_file (fp, errbuf, sizeof (errbuf));
    (void)fclose (fp);
    if (!tab) {
        errfromtoml (error, filename, errbuf);
        errno = EINVAL;
        return NULL;
    }
    return tab;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
