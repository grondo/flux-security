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

#ifndef HAVE_IMPCMD_H
#define HAVE_IMPCMD_H 1

struct imp_state;

typedef int (*imp_cmd_f) (struct imp_state *imp);

/*  Return unprivileged child version of IMP command with `name`, or NULL
 *   if no such command found.
 */
imp_cmd_f imp_cmd_find_child (const char *name);

/*  Return privileged parent version of IMP command with `name`, or NULL
 *   if no such command found.
 */
imp_cmd_f imp_cmd_find_parent (const char *name);

#endif /* !HAVE_IMPCMD_H */
