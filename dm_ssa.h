/*
 * Copyright (c) 2011, Ed Robbins <edd.robbins@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __DM_SSA_H
#define __DM_SSA_H

#include "common.h"
#include "dm_dom.h"
#include "dm_cfg.h"

struct dm_ssa_index {
	enum ud_type		  reg;
	int			  index;
	struct dm_cfg_node	**def_nodes; /* Nodes where var defined */
	int			  dn_count;
	struct dm_cfg_node	**phi_nodes; /* Nodes with phi funcs for var*/
	int			  pn_count;
};

void	dm_ssa_find_var_defs();
void	dm_ssa_index_init();
int	dm_cmd_ssa(char **args);

#endif

