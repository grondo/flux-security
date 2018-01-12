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

imp_conf_t * imp_conf_load (const char *pattern)
{
    imp_conf_t *conf = imp_conf_create ();
    if (conf && pattern && cf_update_glob (conf->cf, pattern, NULL) < 0) {
        imp_conf_destroy (conf);
        return (NULL);
    }
    return (conf);
}

const cf_t * imp_conf_get_table (imp_conf_t *conf, const char *key)
{
    if (!key)
        return (conf->cf);
    if (cf_typeof (conf->cf) == CF_TABLE)
        return cf_get_in (conf->cf, key);
    return (NULL);
}

const cf_t * imp_conf_cf (imp_conf_t *conf)
{
    return (conf->cf);
}



/*
 * vi: ts=4 sw=4 expandtab
 */
