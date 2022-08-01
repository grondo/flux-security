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
#include <config.h>
#endif

#include <stdio.h>
#include "impcmd.h"

int imp_cmd_version (struct imp_state *imp __attribute__ ((unused)),
                     struct kv *kv __attribute__ ((unused)))
{
    bool n = 0;
    printf ("flux-imp v%s\n", PACKAGE_VERSION);
#if HAVE_PAM
    n += printf ("%s+pam", n ? " " : "");
#endif
    if (n)
        printf ("\n");
    return (0);
}

/* vi: ts=4 sw=4 expandtab
 */
