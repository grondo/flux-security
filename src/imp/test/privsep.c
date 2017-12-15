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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include "imp_log.h"
#include "sudosim.h"
#include "privsep.h"

#include "src/libtap/tap.h"

static void child (privsep_t *ps, void *arg __attribute__ ((unused)))
{
    bool rv;
    uid_t euid = geteuid ();
    uid_t uid = getuid ();
    int len = sizeof (uid_t);

    int n = privsep_write (ps, &euid, len);
    if (n != len) {
        diag ("privsep_write: euid: %s", strerror (errno));
        exit (1);
    }
    n = privsep_write (ps, &uid, len);
    if (n != len) {
        diag ("privsep_write: uid: %s", strerror (errno));
        exit (1);
    }

    /*  Can't use TAP output here (numbering would be incorrect),
     *   instead send result directly to parent over the privsep connection.
     */
    rv = privsep_is_child (ps);
    n = privsep_write (ps, &rv, sizeof (rv));
    if (n != sizeof (rv)) {
        diag ("privsep write: bool: %s", strerror (errno));
        exit (1);
    }

    rv = privsep_is_parent (ps);
    n = privsep_write (ps, &rv, sizeof (rv));
    if (n != sizeof (rv)) {
        diag ("privsep write: bool: %s", strerror (errno));
        exit (1);
    }
    /*  Child exits on return */
}

static int log_diag (int level, const char *str,
                     void *arg __attribute__ ((unused)))
{
    diag ("privsep: %s: %s\n", imp_log_strlevel (level), str);
    return (0);
}

int main (void)
{
    privsep_t *ps = NULL;
    uid_t uid, euid;
    bool result;
    ssize_t len = sizeof (uid_t);

    /*  Privsep code uses imp log for errors, so need to initialize here
     */
    imp_openlog ();
    imp_log_add ("diag", IMP_LOG_DEBUG, log_diag, NULL);

    if (sudo_simulate_setuid () < 0)
        BAIL_OUT ("Failed to simulate setuid under sudo");

    if (geteuid () == getuid ()) {
        plan (SKIP_ALL, "Privsep test needs to be run setuid");
        return (0);
    }

    plan (NO_PLAN);

    ok ((ps = privsep_init (child, NULL)) != NULL, "privsep_init");
    if (ps == NULL)
        BAIL_OUT ("privsep_init failed");

    ok (privsep_is_parent (ps), "privsep_is_parent returns true in parent");

    ok (privsep_read (ps, &euid, len) == len,
        "privsep_read: euid from child");
    ok (privsep_read (ps, &uid, len) == len,
        "privsep_read: uid from child");

    ok (euid == getuid (),
        "child has effective uid of parent real uid (euid=%ld)", (long) euid);
    ok (uid == getuid (),
        "child has real uid of parent (uid=%ld)", (long) uid);


    ok (privsep_read (ps, &result, sizeof (result)) == sizeof (result),
        "privsep read: result of privsep_is_child");
    ok (result == true, "privsep_is_child in child returns true");
    ok (privsep_read (ps, &result, sizeof (result)) == sizeof (result),
        "privsep read: result of privsep_is_parent");
    ok (result == false, "privsep_is_parent in child returns false");

    ok (!privsep_is_child (ps), "privsep_is_child in parent returns false");


    ok (geteuid() == 0,
        "parent retains effective uid == 0");

    ok (privsep_destroy (ps) == 0, "privsep child exited normally");

    imp_closelog ();
    done_testing ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */
