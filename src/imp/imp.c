/*****************************************************************************\
 *  Copyright (c) 2017 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/libutil/cf.h"
#include "imp_log.h"

extern const char *imp_config_pattern;

struct imp_state {
    int        argc;
    char     **argv;        /* cmdline arguments from main() */

    cf_t      *conf;        /* IMP configuration */

    uid_t      ruid;        /* Real user id at program startup */
    uid_t      euid;        /* Effective user id at program startup */
    uid_t      rgid;        /* Real group id at program startup */
    uid_t      egid;        /* Effective group id at program startup */
};

/*  Static prototypes:
 */
static void initialize_logging ();
static int  imp_state_init (struct imp_state *imp, int argc, char **argv);
static cf_t * imp_conf_load (const char *pattern);

int main (int argc, char *argv[])
{
    struct imp_state imp;

    initialize_logging ();

    if (imp_state_init (&imp, argc, argv) < 0)
        imp_die (1, "Initialization error");

    /*  Configuration:
     */
    if (!(imp.conf = imp_conf_load (imp_config_pattern)))
        imp_die (1, "Failed to load configuration");

    /*  Audit subsystem initialization
     */
    // Skip.

    /*  Security architecture initialization
     */
    // Skip.

    /*  Parse command line and run subcommand
     */
    // Skip.

    imp_closelog ();
    exit (0);
}

static int log_stderr (int level, const char *str,
                       void *arg __attribute__ ((unused)))
{
    if (level == IMP_LOG_INFO)
        fprintf (stderr, "flux-imp: %s\n", str);
    else
        fprintf (stderr, "flux-imp: %s: %s\n", imp_log_strlevel (level), str);
    return (0);
}

static void initialize_logging (void)
{
    imp_openlog ();
    if (imp_log_add ("stderr", IMP_LOG_INFO, log_stderr, NULL) < 0) {
        fprintf (stderr, "flux-imp: Fatal: Failed to initialize logging.\n");
        exit (1);
    }
}

static int imp_state_init (struct imp_state *imp, int argc, char *argv[])
{
    memset (imp, 0, sizeof (*imp));
    imp->euid = geteuid ();
    imp->ruid = getuid ();
    imp->egid = getegid ();
    imp->rgid = getgid ();
    imp->argc = argc;
    imp->argv = argv;
    return (0);
}

/*
 *  Load IMP configuration from glob(7) `pattern`. Fatal error if configuration
 *   fails to load.
 */
static cf_t * imp_conf_load (const char *pattern)
{
    int rc;
    struct cf_error err;
    cf_t *cf = NULL;

    if (pattern == NULL)
        imp_die (1, "imp_conf_load: Internal error");

    if (!(cf = cf_create ()))
        return (NULL);

    memset (&err, 0, sizeof (err));
    if ((rc = cf_update_glob (cf, pattern, &err)) < 0) {
        imp_warn ("loading config: %s: %d: %s",
                 err.filename ? err.filename : "none",
                 err.lineno,
                 err.errbuf);
        cf_destroy (cf);
        return (NULL);
    }
    else if (rc == 0) {
        imp_warn ("%s: No config file(s) found");
        cf_destroy (cf);
        return (NULL);
    }
    return (cf);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
