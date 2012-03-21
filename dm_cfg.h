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
	int			disjunctive;
};

struct dm_cfg_node {
	NADDR			  start;
	NADDR			  end;
	struct dm_cfg_node	**children;
	struct dm_cfg_node	**parents;
	int			  hell_node;
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
	struct instruction	**instructions; /* Instructions in this node */
	int			  i_count;
};

struct phi_function {
	enum ud_type		   var;
	int			   arguments;
	int			   index;
	int			  *indexes;
	struct type_constraint	***constraints;
	int			  *c_counts;
	int			   d_count;
};

struct instruction {
	struct ud		   ud;
	int			   index[3][2];
	int			   cast[3];
	struct type_constraint	***constraints; /* Constraints */
	int			  *c_counts; /*# conjunctions */
	int			   d_count; /*# disjunctions */
};

enum type_class {
	T_REGISTER,
	T_PTR,
	T_BASIC,
	T_ALPHA,
	T_ARRAY,
	T_STRUCT
};

enum basic_type {
	BT_CHAR = 8,	/* 8 bit */
	BT_SHORT = 16,	/* 16 bit */
	BT_INT = 32,	/* 32 bit */
	BT_LONG = 64	/* 64 bit */
};

struct lval {
	int	_signed;
	int	size;
	union {
		int8_t		sbyte;
		uint8_t		ubyte;
		int16_t		sword;
		uint16_t	uword;
		int32_t		sdword;
		uint32_t	udword;
		int64_t		sqword;
		uint64_t	uqword;
	} lval_u;
};

struct type_descriptor {
	enum type_class		  c_type;
	/* If register */
	enum ud_mnemonic_code	  reg;
	int			  r_index;
	/* If ptr */
	struct type_descriptor	 *p_type;
	/* If struct */
	struct type_descriptor	**types;
	struct lval		 *offsets;
	int			  t_count;
	/* If basic type */
	enum basic_type		  b_type;
	/* If alpha */
	int			  a_index;
	/* If array */
	struct type_descriptor	 *a_type;
	/* */
	struct type_descriptor	 *set;
	struct type_constraint	 *def;
};

struct type_constraint {
	struct type_descriptor	  *left;
	struct type_descriptor	  *right;
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

int			dm_is_target_in_text(NADDR addr);
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
