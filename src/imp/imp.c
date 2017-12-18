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
//static void print_version (void);

bool imp_is_setuid (struct imp_state *imp)
{
    return (imp->euid == 0 && imp->ruid > 0);
}


int main ()
{
    struct imp_state imp;

    /*  Initialize early logging to stderr only. Abort on failure.
     */
    initialize_logging ();

    if (imp_state_init (&imp) < 0)
        imp_die (1, "Initialization error");

    if (!(imp.config = imp_conf_load (IMP_CONFIG_PATH)))
        imp_die (1, "Failed to load IMP configuration.");

    imp_conf_destroy (imp.config);

    imp_closelog ();
    exit (0);
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
