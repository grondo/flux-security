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
#ifndef HAVE_PRIVSEP_H
#define HAVE_PRIVSEP_H 1

#include <stdbool.h>

typedef struct privsep privsep_t;

typedef void (*privsep_child_f) (privsep_t *ps, void *arg);

/*  Spawn an unprivliged child running child_fn from a setuid program,
 *   connected to the current process with pipes for IPC.
 *
 *  Parent returns valid privsep_t on success, child calls function fn
 *   and does not return.
 */
privsep_t * privsep_init (privsep_child_f fn, void *arg);

/*  Free memory associated with privsep handle and close associated
 *   file descriptors to parent/child. If this is the parent process,
 *   it will wait for the child to exit.
 */  
int privsep_destroy (privsep_t *ps);

/*  Return true if running in child.
 */
bool privsep_is_child (privsep_t *ps);

/*  Return true if running in parent
 */
bool privsep_is_parent (privsep_t *ps);

/*
 *  Read up to count bytes from privsep connection into buffer buf.
 *  Returns number of bytes read into buf or -1 on error.
 */
ssize_t privsep_read (privsep_t *ps, void *buf, size_t count);

/*
 *  Write count bytes from buf over privsep channel in `ps`.
 *  Returns number of bytes written (always == count) or -1 on failure
 */
ssize_t privsep_write (privsep_t *ps, const void *buf, size_t count);

#endif /* !HAVE_PRIVSEP_H */

