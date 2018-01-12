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
#include <assert.h>

#include "imp_log.h"
#include "privsep.h"
#include "sudosim.h"
#include "impcmd.h"
#include "conf.h"

#define IMP_CONFIG_PATH "imp.conf.d/*.toml"

struct imp_state {
    int argc;              /* Cmdline arguments from main() */
    char **argv;

    cf_t  *config;         /* IMP configuration */
    privsep_t *ps;         /* Privilege separation handle */

    uid_t ruid;            /* Real uid at program startup */
    uid_t euid;            /* Effective uid at program startup */
    uid_t rgid;            /* Real gid at program startup */
    uid_t egid;            /* Effective gid at program startup */

    privsep_t *privsep;    /* Privilege separation handle if running setuid */
};


/*  Static prototypes:
 */
static bool imp_is_setuid ();
static bool imp_is_privileged ();
static void initialize_logging (void);
static int  imp_state_init (struct imp_state *imp, int argc, char **argv);
static const cf_t * imp_config_get (imp_conf_t *conf, const char *key);
static void initialize_sudo_support (imp_conf_t *conf);

static void imp_child (privsep_t *ps, void *arg);
static void imp_parent (struct imp_state *imp);

//static void print_version (void);
int main (int argc, char *argv[])
{
    struct imp_state imp;

    /*  Initialize early logging to stderr only. Abort on failure.
     */
    initialize_logging ();

    if (imp_state_init (&imp, argc, argv) < 0)
        imp_die (1, "Initialization error");

    if (!(imp.config = imp_conf_load (IMP_CONFIG_PATH)))
        imp_die (1, "Failed to load IMP configuration.");

    if (imp_is_privileged ()) {

        /*  Simulate setuid under sudo if configured */
        initialize_sudo_support (imp.config);

        if (!imp_is_setuid ())
            imp_die (1, "Refusing to run as root");

        /*
         *  Initialize privilege separation (required for now)
         */
        if (!(imp.ps = privsep_init (imp_child, &imp)))
            imp_die (1, "Privilege separation initialization failed");
        imp_parent (&imp);

        if (privsep_destroy (imp.ps) < 0)
            imp_warn ("privsep_destroy: %s", strerror (errno));
    }
    else {
        /*  We can only run unprivileged */
        imp_child (NULL, &imp);
    }

    imp_conf_destroy (imp.config);
    imp_closelog ();
    exit (0);
}

/*  Simulate setuid installation when run under sudo if "allow-sudo" is
 *   set to true in configuration. If `allow-sudo` is not set and the
 *   process appears to be run under sudo, or the sudo simulate call fails
 *   then this is a fatal error.
 */
static void initialize_sudo_support (imp_conf_t *conf)
{
    const cf_t *cf;
    if (sudo_is_active ()) {
        if (!(cf = imp_config_get (conf, "allow-sudo")) || !cf_bool (cf))
            imp_die (1, "sudo support not enabled");
        else if (sudo_simulate_setuid () < 0)
            imp_die (1, "Failed to enable sudo support");
    }
}

static const cf_t * imp_config_get (imp_conf_t *conf, const char *key)
{
    char *s, *cpy = strdup (key);
    const cf_t *cf = imp_conf_cf (conf);
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

bool imp_is_privileged ()
{
    return (geteuid() == 0);
}

bool imp_is_setuid ()
{
    return (geteuid() == 0 && getuid() > 0);
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

/*
 *  IMP unprivileged child. Gather input and send request to parent.
 */
static void imp_child (privsep_t *ps, void *arg)
{
    struct imp_state *imp = arg;
    imp_cmd_f cmd = NULL;
    assert (imp != NULL);
    ps = ps;

    if (imp->argc <= 1)
        imp_die (1, "command required");

    if (!(cmd = imp_cmd_find_child (imp->argv[1])))
        imp_die (1, "Unknown IMP command: %s", imp->argv[1]);

    if (((*cmd) (imp)) < 0)
        exit (1);
}

static void imp_parent (struct imp_state *imp)
{
    char buf [4096];
    if (privsep_read (imp->ps, buf, sizeof (buf)) < 0)
        imp_die (1, "privsep_read: %s", strerror (errno));
}

/*
 * vi: ts=4 sw=4 expandtab
 */
