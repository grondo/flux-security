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
#include <string.h>

#include "impcmd.h"

struct impcmd {
    const char *name;
    imp_cmd_f child_fn;
    imp_cmd_f parent_fn;
};

/*  List of supported imp commands, curated by hand for now.
 *   For each named command, the `child_fn` runs unprivileged and the
 *   `parent_fn` runs privileged. The child function communicates to
 *   the parent using privsep_write/read.
 *
 */
static struct impcmd impcmds[] = {
	{ NULL, NULL, NULL}
};

static struct impcmd * imp_cmd_lookup (const char *name)
{
    struct impcmd *cmd = &impcmds[0];
    while (cmd->name != NULL) {
        if (strcmp (name, cmd->name) == 0)
            return (cmd);
        cmd++;
    }
    return (NULL);
}

imp_cmd_f imp_cmd_find_child (const char *name)
{
    struct impcmd *cmd = imp_cmd_lookup (name);
    if (cmd)
        return (cmd->child_fn);
    return (NULL);
}

imp_cmd_f imp_cmd_find_parent (const char *name)
{
    struct impcmd *cmd = imp_cmd_lookup (name);
    if (cmd)
        return (cmd->parent_fn);
    return (NULL);
}

/* vi: ts=4 sw=4 expandtab */
