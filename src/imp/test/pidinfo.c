/************************************************************\
 * Copyright 2022 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "pidinfo.h"

#include "src/libtap/tap.h"

int main (void)
{
    struct pid_info *p;

    ok (pid_info_create (0) == NULL && errno == EINVAL,
        "pid_info_create (0) fails with EINVAL");

    ok ((p = pid_info_create (getpid ())) != NULL,
        "pid_info_create (getpid ()) works");
    ok (p->pid == getpid (),
        "p->pid is expected");
    ok (p->pid_owner == getuid (),
        "p->pid_owner is expected");
    diag ("p->cg_path = %s", p->cg_path);
    diag ("p->cg_owner = %d", (int) p->cg_owner);
    pid_info_destroy (p);

    ok ((p = pid_info_create (-getpid ())) != NULL,
        "pid_info_create (-getpid ()) works");
    ok (p->pid == getpid (),
        "p->pid is expected");
    ok (p->pid_owner == getuid (),
        "p->pid_owner is expected");
    diag ("p->cg_path = %s", p->cg_path);
    diag ("p->cg_owner = %d", (int) p->cg_owner);
    pid_info_destroy (p);

    done_testing ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */
