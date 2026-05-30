/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* cgroup-device-apply - apply BPF device containment to a cgroup
 *
 * Usage: cgroup-device-apply CGROUP_PATH
 *
 * Read a JSON object from stdin containing optional DevicePolicy and
 * DeviceAllow keys (RFC 15) and attach a BPF cgroup device filter to
 * the cgroup at CGROUP_PATH.  Must be run as root.
 *
 * Exit codes:
 *   0  success, including no-op when DeviceAllow is absent or empty
 *   1  runtime failure (JSON parse error, cgroup access, BPF error)
 *   2  usage error (missing argument, invalid CGROUP_PATH form)
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "imp_log.h"
#include "cgroup.h"
#include "cgroup_device.h"
#include "exec/device.h"

static int stderr_logger (int level __attribute__((unused)),
                          const char *msg,
                          void *arg __attribute__((unused)))
{
    fprintf (stderr, "cgroup-device-apply: %s\n", msg);
    return 0;
}

int main (int argc, char *argv[])
{
    json_t *input = NULL;
    json_error_t err;
    struct device_allow *da = NULL;
    struct cgroup_info *cgroup = NULL;
    int rc = 1;

    imp_openlog ();
    imp_log_add ("stderr", IMP_LOG_WARNING, stderr_logger, NULL);

    if (argc != 2) {
        fprintf (stderr, "Usage: cgroup-device-apply CGROUP_PATH\n");
        return 2;
    }
    if (!(cgroup = cgroup_info_from_path (argv[1]))) {
        fprintf (stderr,
                 "cgroup-device-apply: %s: %s\n",
                 argv[1],
                 strerror (errno));
        return 2;
    }
    if (!(input = json_loadf (stdin, 0, &err))) {
        fprintf (stderr,
                 "cgroup-device-apply: error reading JSON input: %s\n",
                 err.text);
        goto done;
    }
    if (device_allow_from_options (input, &da) < 0) {
        fprintf (stderr,
                 "cgroup-device-apply: failed to parse device policy: %s\n",
                 strerror (errno));
        goto done;
    }
    if (!da) {
        rc = 0;  /* no-op: no containment configured */
        goto done;
    }
    if (cgroup_device_apply (cgroup, da) < 0) {
        fprintf (stderr,
                 "cgroup-device-apply: failed to apply device policy: %s\n",
                 strerror (errno));
        goto done;
    }
    rc = 0;
done:
    device_allow_destroy (da);
    cgroup_info_destroy (cgroup);
    json_decref (input);
    return rc;
}

/* vi: ts=4 sw=4 expandtab
 */
