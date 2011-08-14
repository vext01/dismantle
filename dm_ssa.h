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
	int			  count;
	int			 *stack;
	int			  s_size;
	struct dm_cfg_node	**def_nodes; /* Nodes where var defined */
	int			  dn_count;
	struct dm_cfg_node	**phi_nodes; /* Nodes with phi funcs for var*/
	int			  pn_count;
};

void		dm_ssa_free();
struct ptrs*	mergeSort(struct ptrs *list);
struct ptrs*	merge(struct ptrs *left, struct ptrs *right);
struct ptrs*	split(struct ptrs *list);
void		dm_print_ssa();
void		dm_ssa_index_stack_push(enum ud_type reg, int i);
int		dm_ssa_index_stack_pop(enum ud_type reg);
void		dm_rename_variables(struct dm_cfg_node *n);
void		gen_operand_ssa(struct ud* u, struct ud_operand* op, int syn_cast,
		    int *index);
void		dm_translate_intel_ssa(struct instruction *insn);
void		dm_place_phi_functions();
void		dm_ssa_find_var_defs();
void		dm_ssa_index_init();
int		dm_cmd_ssa(char **args);
int		dm_array_contains(struct dm_cfg_node **list, int c,
		    struct dm_cfg_node *term);

#endif

