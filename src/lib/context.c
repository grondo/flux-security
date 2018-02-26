/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
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
#  include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "src/libutil/cf.h"
#include "src/libutil/hash.h"

#include "context.h"

struct flux_security {
    cf_t *config;
    hash_t aux;
    char error[200];
    int errnum;
};

struct aux_item {
    void *data;
    void (*free)(void *);
};

static void aux_item_destroy (struct aux_item *item)
{
    if (item) {
        if (item->free)
            item->free (item->data);
        free (item);
    }
}

/* Update 'e' if non-NULL.
 * If 'fmt' is non-NULL, build message; otherwise use strerror (errno).
 */
static void ferr (flux_security_t *ctx, const char *fmt, ...)
{
    if (ctx) {
        size_t sz = sizeof (ctx->error);
        ctx->errnum = errno;
        if (fmt) {
            va_list ap;
            va_start (ap, fmt);
            vsnprintf (ctx->error, sz, fmt, ap);
            va_end (ap);
        }
        else
            snprintf (ctx->error, sz, "%s", strerror (ctx->errnum));
        errno = ctx->errnum;
    }
}

flux_security_t *flux_security_create (int flags)
{
    flux_security_t *ctx;

    if (flags != 0) { // not used yet
        errno = EINVAL;
        return NULL;
    }
    if (!(ctx = calloc (1, sizeof (*ctx))))
        return NULL;
    ctx->aux = hash_create (0, (hash_key_f) hash_key_string,
                               (hash_cmp_f) strcmp,
                               (hash_del_f) aux_item_destroy);

    return ctx;
}

void flux_security_destroy (flux_security_t *ctx)
{
    if (ctx) {
        if (ctx->aux)
            hash_destroy (ctx->aux);
        cf_destroy (ctx->config);
        free (ctx);
    }
}

const char *flux_security_last_error (flux_security_t *ctx)
{
    return (ctx && *ctx->error) ? ctx->error : NULL;
}

int flux_security_last_errnum (flux_security_t *ctx)
{
    return ctx ? ctx->errnum : 0;
}


int flux_security_configure (flux_security_t *ctx, const char *pattern)
{
    struct cf_error cfe;
    int n;

    if (!ctx) {
        errno = EINVAL;
        ferr (ctx, NULL);
        return -1;
    }
    if (!pattern)
        pattern = INSTALLED_CF_PATTERN;
    if (!(ctx->config = cf_create ())) {
        ferr (ctx, NULL);
        return -1;
    }
    if ((n = cf_update_glob (ctx->config, pattern, &cfe)) < 0) {
        if (cfe.lineno != -1)
            ferr (ctx, "%s::%d: %s", cfe.filename, cfe.lineno, cfe.errbuf);
        else
            ferr (ctx, "%s", cfe.errbuf);
        return -1;
    }
    if (n == 0) {
        errno = EINVAL;
        ferr (ctx, "pattern %s matched nothing", pattern);
        return -1;
    }
    return 0;
}

int flux_security_aux_set (flux_security_t *ctx, const char *name,
                           void *data, void (*freefun)(void *))
{
    struct aux_item *item;

    if (!ctx || !name || !data) {
        errno = EINVAL;
        ferr (ctx, NULL);
        return -1;
    }
    if (!(item = calloc (1, sizeof (*item)))) {
        ferr (ctx, NULL);
        return -1;
    }
    item->data = data;
    item->free = freefun;
    if (hash_insert (ctx->aux, name, item) == NULL) {
        security_error (ctx, NULL);
        free (item);
        errno = flux_security_last_errnum (ctx);
        return -1;
    }
    return 0;
}

void *flux_security_aux_get (flux_security_t *ctx, const char *name)
{
    struct aux_item *item;

    if (!ctx || !name) {
        errno = EINVAL;
        ferr (ctx, NULL);
        return NULL;
    }
    if (!(item = hash_find (ctx->aux, name)))
        return NULL;
    return item->data;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
