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

#ifndef HAVE_SUDOSIM_H
#define HAVE_SUDOSIM_H 1

#include <stdbool.h>

/*  If current process was run under sudo, return the user name as recorded
 *   in the SUDO_USER environment variable. Return NULL otherwise.
 */
const char * sudo_user_name (void);

/*  Return true if it appears the current process is running under sudo, i.e.
 *   real UID is 0, and SUDO_USER environment variable is set.
 */
bool sudo_is_active (void);

/*  If running under sudo by evidence of real UID == 0 and SUDO_USER set,
 *   adapt process credentials to simulate a setuid program. That is,
 *   set the real UID/GID to that of SUDO_USER and leave effective/saved
 *   UIDs as root.
 *
 *  Returns 0 on success or if SUDO not active (in which case nothing was
 *   done), or < 0 if some failure occurred (should be fatal).
 */
int sudo_simulate_setuid (void);

#endif /* !HAVE_SUDOSIM_H */
