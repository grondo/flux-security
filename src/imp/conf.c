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
#include <errno.h>
#include <glob.h>

#include "src/libutil/cf.h"

#include "imp_log.h"
#include "conf.h"

struct imp_conf {
    cf_t * cf;
};

static imp_conf_t *imp_conf_create (void)
{
    imp_conf_t *conf = calloc (1, sizeof (*conf));
    if (!conf || !(conf->cf = cf_create())) {
        free (conf);
        return NULL;
    }
    return conf;
}

void imp_conf_destroy (imp_conf_t *conf)
{
    cf_destroy (conf->cf);
    free (conf);
}

static int imp_conf_update (imp_conf_t *conf, const char *filename)
{
    struct cf_error error;

    /*  XXX: sanity check file access permissions before reading it
     *       into config.
     */
    if (cf_update_file (conf->cf, filename, &error)) {
        imp_warn ("%s: %d: %s", filename, error.lineno, error.errbuf);
        return (-1);
    }

    return (0);
}

static int errf (const char *msg, int errnum)
{
    imp_warn ("glob: %s: %s", msg, strerror (errnum));
    return (0);
}

static int imp_conf_update_glob (imp_conf_t *conf, const char *pattern)
{
    int count = -1;
    glob_t gl;
    size_t i;

    int rc = glob (pattern, GLOB_ERR, errf, &gl);
    switch (rc) {
        case 0:
            count = 0;
            for (i = 0; i < gl.gl_pathc; i++) {
                if (imp_conf_update (conf, gl.gl_pathv[i]) == 0)
                    count++;
            }
            break;
        case GLOB_NOMATCH:
            break;
        case GLOB_NOSPACE:
            imp_die (1, "imp_conf_update: Out of memory");
        case GLOB_ABORTED:
            imp_warn ("imp_conf_update: cannot read dir %s", pattern);
            break;
        default:
            imp_warn ("imp_conf_update: unknown glob(3) return code = %d", rc);
            break;

    }
    globfree (&gl);
    return (count);
}

imp_conf_t * imp_conf_load (const char *pattern)
{
    imp_conf_t *conf = imp_conf_create ();
    if (conf && pattern && imp_conf_update_glob (conf, pattern) < 0) {
        imp_conf_destroy (conf);
        return (NULL);
    }
    return (conf);
}

cf_t * imp_conf_get_table (imp_conf_t *conf, const char *key)
{
    if (!key)
        return (conf->cf);
    if (cf_typeof (conf->cf) == CF_TABLE)
        return cf_get_in (conf->cf, key);
    return (NULL);
}

cf_t * imp_conf_cf (imp_conf_t *conf)
{
    return (conf->cf);
}



/*
 * vi: ts=4 sw=4 expandtab
 */
