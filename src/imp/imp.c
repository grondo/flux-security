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
#include <sys/types.h>
#include <errno.h>

#include "imp_log.h"
#include "privsep.h"
#include "sudosim.h"
#include "conf.h"

#define IMP_CONFIG_PATH "imp.conf.d/*.toml"

struct imp_state {
    cf_t  *config;         /* IMP configuration */

    uid_t ruid;            /* Real uid at program startup */
    uid_t euid;            /* Effective uid at program startup */
    uid_t rgid;            /* Real gid at program startup */
    uid_t egid;            /* Effective gid at program startup */

    privsep_t *privsep;    /* Privilege separation handle if running setuid */
};


/*  Static prototypes:
 */
static void initialize_logging (void);
static int  imp_state_init (struct imp_state *imp);
static cf_t * imp_config_get (imp_conf_t *conf, const char *key);
//static void print_version (void);

bool imp_is_setuid (struct imp_state *imp)
{
    return (imp->euid == 0 && imp->ruid > 0);
}


int main (int argc, char *argv[])
{
    cf_t *cf;
    struct imp_state imp;

    /*  Initialize early logging to stderr only. Abort on failure.
     */
    initialize_logging ();

    if (imp_state_init (&imp) < 0)
        imp_die (1, "Initialization error");

    if (!(imp.config = imp_conf_load (IMP_CONFIG_PATH)))
        imp_die (1, "Failed to load IMP configuration.");

    if (argc >= 2) {
        const char *key = argv[1];
        time_t t;
        if (!(cf = imp_config_get (imp.config, key)))
            imp_die (1, "%s: %s", key, strerror (errno));
        switch (cf_typeof (cf)) {
            case CF_INT64:
                printf ("%s = %ju\n", key, cf_int64 (cf));
                break;
            case CF_DOUBLE:
                printf ("%s = %f\n", key, cf_double (cf));
                break;
            case CF_BOOL:
                printf ("%s = %s\n", key, cf_bool (cf) ? "true":"false");
                break;
            case CF_STRING:
                printf ("%s = %s\n", key, cf_string (cf));
                break;
            case CF_TIMESTAMP:
                t = cf_timestamp (cf);
                printf ("%s = %s\n", key, ctime (&t));
                break;
            case CF_TABLE:
                printf ("%s = <table>\n", key);
                break;
            case CF_ARRAY:
                printf ("%s = <array>\n", key);
                break;
            default:
                printf ("Unknown key type for %s", key);
                break;
        }
    }

    imp_conf_destroy (imp.config);

    imp_closelog ();
    exit (0);
}

static cf_t * imp_config_get (imp_conf_t *conf, const char *key)
{
    char *s, *cpy = strdup (key);
    cf_t *cf = imp_conf_cf (conf);
    s = strtok (cpy, ".");
    while (s) {
        if (!(cf = cf_get_in (cf, s)))
            goto out;
        s = strtok (NULL, ".");
    }
out:
    free (cpy);
    return (cf);
}

static int imp_state_init (struct imp_state *imp)
{
    memset (imp, 0, sizeof (*imp));
    imp->euid = geteuid ();
    imp->ruid = getuid ();
    imp->egid = getegid ();
    imp->rgid = getgid ();
    return (0);
}

#if 0
static void print_version ()
{
    printf ("flux-imp v%s\n", PACKAGE_VERSION);
}
#endif

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

/*
 * vi: ts=4 sw=4 expandtab
 */
