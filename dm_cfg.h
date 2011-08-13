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

#ifndef __CFG_H
#define __CFG_H

#include "common.h"
#include "dm_dis.h"


struct dm_instruction_se {
	enum ud_mnemonic_code	instruction;
	int			write;
	int			jump;
	int			ret;
};

struct dm_cfg_node {
	NADDR			  start;
	NADDR			  end;
	struct dm_cfg_node	**children;
	struct dm_cfg_node	**parents;
	int			  p_count;
	int			  visited;
	int			  pre;     /* Pre-order position */
	int			  post;    /* Post-order position */
	int			  rpost;   /* Reverse Post-order position */
	struct dm_cfg_node	 *idom;	   /* Immediate dominator of node */
	struct dm_cfg_node	**df_set;  /* Dominance frontier set of node */
	int			  df_count;
	enum ud_type		 *def_vars;/* Vars defined in this node */
	int			  dv_count;
	struct phi_function	 *phi_functions;/* Vars requiring phi funcs */
	int			  pf_count;
	struct instruction	**instructions;
	int			  i_count;
	char			 *ssa_output;
};

struct phi_function {
	enum ud_type		  var;
	int			  arguments;
	int			  index;
	int			 *indexes;
};

struct instruction {
	struct ud		  ud;
	int			  index[3][2];
};

/*
 * A linked list of all the CFG blocks so we can free them at the end.
 * XXX queue.h
 */
struct ptrs {
	void		*ptr;
	struct ptrs	*next;
};

void			dm_check_cfg_consistency();
void			dm_instruction_se_init();
int			dm_cmd_cfg(char **args);

struct dm_cfg_node*	dm_recover_cfg();
void			dm_init_cfg();
struct dm_cfg_node*	dm_new_cfg_node(NADDR nstart, NADDR nend);
void			dm_print_cfg();
void			dm_graph_cfg();
void			dm_free_cfg();
struct dm_cfg_node*	dm_gen_cfg_block(struct dm_cfg_node *node);

void			dm_dfw(struct dm_cfg_node *node);
struct dm_cfg_node*	dm_get_unvisited_node();
void			dm_depth_first_walk(struct dm_cfg_node *cfg);

void			dm_add_parent(struct dm_cfg_node *node,
			    struct dm_cfg_node *parent);
struct dm_cfg_node*	dm_split_cfg_block(struct dm_cfg_node *node,
			    NADDR addr);
struct dm_cfg_node*	dm_find_cfg_node_starting(NADDR addr);
struct dm_cfg_node*	dm_find_cfg_node_ending(NADDR addr);
struct dm_cfg_node*	dm_find_cfg_node_containing(NADDR addr);

#endif
