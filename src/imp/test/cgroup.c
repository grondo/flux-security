/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cgroup.h"
#include "src/libtap/tap.h"

static void test_null_path (void)
{
    errno = 0;
    ok (cgroup_info_from_path (NULL) == NULL && errno == EINVAL,
        "cgroup_info_from_path: NULL path fails with EINVAL");
}

static void test_relative_path (void)
{
    errno = 0;
    ok (cgroup_info_from_path ("sys/fs/cgroup/user.slice") == NULL
        && errno == EINVAL,
        "cgroup_info_from_path: relative path fails with EINVAL");
}

static void test_wrong_prefix (void)
{
    errno = 0;
    ok (cgroup_info_from_path ("/sys/fs/other/user.slice") == NULL
        && errno == EINVAL,
        "cgroup_info_from_path: path not under /sys/fs/cgroup/ fails with EINVAL");
}

static void test_path_too_long (void)
{
    char path[PATH_MAX + 32];
    memset (path, 'a', sizeof (path) - 1);
    path[sizeof (path) - 1] = '\0';
    memcpy (path, "/sys/fs/cgroup/", 15);
    errno = 0;
    ok (cgroup_info_from_path (path) == NULL && errno == ENAMETOOLONG,
        "cgroup_info_from_path: path > PATH_MAX fails with ENAMETOOLONG");
}

static void test_valid_path (void)
{
    const char *p = "/sys/fs/cgroup/user.slice/user-1000.slice";
    struct cgroup_info *cgroup = cgroup_info_from_path (p);
    ok (cgroup != NULL,
        "cgroup_info_from_path: valid path returns non-NULL");
    ok (cgroup && strcmp (cgroup->path, p) == 0,
        "cgroup_info_from_path: path field set correctly");
    cgroup_info_destroy (cgroup);
}

int main (void)
{
    plan (NO_PLAN);

    test_null_path ();
    test_relative_path ();
    test_wrong_prefix ();
    test_path_too_long ();
    test_valid_path ();

    done_testing ();
    return 0;
}

/* vi: ts=4 sw=4 expandtab
 */
