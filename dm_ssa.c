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
#define _GNU_SOURCE
#include "dm_ssa.h"
#include "dm_dwarf.h"

void opr_cast(struct ud* u, struct ud_operand* op);

//extern void mkasm(struct ud* u, const char* fmt, ...);
//extern void opr_cast(struct ud* u, struct ud_operand* op);
//extern const char* ud_reg_tab[];

extern struct dm_instruction_se *instructions;
extern struct ptrs		*p_head;
extern struct ptrs		*p;
extern int			 p_length;

struct dm_ssa_index		*indices = NULL;

int
dm_cmd_ssa(char **args)
{
	NADDR			 addr = cur_addr;
	struct dm_cfg_node	*cfg = NULL;
	(void) args;

	/* Initialise structures */
	dm_init_cfg();

	/* Get CFG */
	cfg = dm_recover_cfg();

	/* Build dominator tree */
	dm_dom(cfg);

	/* Build dominance frontier sets*/
	dm_dom_frontiers();

	/* Initialise register index structure */
	dm_ssa_index_init();

	/* Build lists of variables defined in each node */
	dm_ssa_find_var_defs();

	/* Place phi functions in correct nodes */
	dm_place_phi_functions();

	/* Rename all the variables with SSA indexes */
	dm_rename_variables(cfg);

	/* Print SSA version of the function */
	dm_print_ssa();

	/* Free all memory used */
	dm_free_ssa();

	/* Free dominance frontier sets */
	dm_dom_frontiers_free();

	/* Free all CFG structures */
	dm_free_cfg();

	/* Rewind back */
	dm_seek(addr);

	return (0);
}

/*
 * Free all memory used for SSA
 */
void
dm_ssa_free()
{
	struct dm_cfg_node	*node = NULL;
	int			 i = 0;

	for (i = 0; i < UD_OP_CONST + 1; i++)
		free(indices[i].stack);
	free(indices);

	for (p = p_head; p != NULL; p = p->next) {
		node = (struct dm_cfg_node*)p->ptr;
		if (!node->nonlocal) {
			for (i = 0; i < node->pf_count; i++)
				free(node->phi_functions[i].indexes);
			free(node->phi_functions);
			for (i = 0; i < node->i_count; i++)
				free(node->instructions[i]);
			free(node->instructions);
			free(node->def_vars);
		}
	}
}

/*
 * Disassemble without translating into assembler (just decode instructions)
 */
unsigned int
dm_ssa_disassemble(struct ud* u)
{
	if (ud_input_end(u))
		return 0;

	u->insn_buffer[0] = u->insn_hexcode[0] = 0;

	if (ud_decode(u) == 0)
		return 0;
	return ud_insn_len(u);
}

struct ptrs*
mergeSort(struct ptrs *list)
{
	struct ptrs *right;

	if (list->next == NULL)
	return list;

	right = split(list);

	return merge(mergeSort(list), mergeSort(right));
}

struct ptrs*
merge(struct ptrs *left, struct ptrs *right)
{
	struct dm_cfg_node *left_p = NULL, *right_p = NULL;
	if (left) left_p = (struct dm_cfg_node*)left->ptr;
	if (right) right_p = (struct dm_cfg_node*)right->ptr;
	if (left == NULL)
		return right;
	else if (right == NULL)
		return left;
	else if (left_p->start > right_p->start)
		return merge(right, left);
	else {
		left->next = merge(left->next, right);
		return left;
	}
}

struct ptrs*
split(struct ptrs *list)
{
	struct ptrs *right;

	if ((list == NULL) || (list->next == NULL))
		return NULL;

	right = list->next;
	list->next = right->next;
	right->next = split(list->next);

	return right;
}

/*
 * Print ssa assembler of all blocks
 */
void
dm_print_ssa()
{
	int				 i = 0;
	struct dm_cfg_node		*node = NULL;

	/* Sort blocks in order of starting address */
	p_head = mergeSort(p_head);

	/* Print blocks in ssa assembler */
	for (p = p_head; (p != NULL); p = p->next) {
		node = (struct dm_cfg_node*)p->ptr;
		/* Print header */
		dm_print_block_header(node);
		/* Print phi nodes */
		for (i = 0; i < node->pf_count; i++) {
			dm_phi_remove_duplicates(&node->phi_functions[i]);
			dm_print_phi_function(&node->phi_functions[i]);
			printf("\n");
		}
		/* Print standard instructions */
		for (i = 0; i < node->i_count; i++) {
			dm_print_ssa_instruction(node->instructions[i]);
			printf("\n");
		}
	}
}

/*
 * Print the header of a CFG block
 */
int
dm_print_block_header(struct dm_cfg_node *node)
{
	int length = 0;
	struct dm_dwarf_sym_cache_entry *sym = NULL;
	if (dm_dwarf_find_sym_at_offset(node->start, &sym) == DM_OK)
		length += printf("%sBlock %d (%s):\n%s", ANSII_LIGHTBLUE, node->post,
		    sym->name, ANSII_WHITE);
	else
		length += printf("%sBlock %d:\n%s", ANSII_LIGHTBLUE, node->post,
		    ANSII_WHITE);
	return length;
}

/*
 * Print a phi function
 */
int
dm_print_phi_function(struct phi_function *phi)
{
	int	 i = 0, length = 0, length2 = 0, newlines = 0;
	char	*temp = NULL, *temp2 = NULL;

	printf("%s", ANSII_GREEN);
	length2 += asprintf(&temp, "%39smov %s_%d, phi(", "", ud_reg_tab[phi->var - 1], phi->index);

	for (i = 0; i < phi->arguments; i++){
		temp2 = temp;
		length2 += asprintf(&temp, "%s%s_%d", temp, ud_reg_tab[phi->var - 1], phi->indexes[i]);
		free(temp2);
		if (i != phi->arguments - 1) {
			temp2 = temp;
			length2 += asprintf(&temp, "%s, ", temp);
			free(temp2);
			if (length2 > 200) {
				printf("%-81s\n", temp);
				free(temp);
				asprintf(&temp, "%43s", "");
				length2 = 0;
				newlines++;
			}
		}
	}
	temp2 = temp;
	asprintf(&temp, "%s)", temp);
	free(temp2);
	length += printf("%-81s", temp);
	printf(ANSII_WHITE);
	free(temp);
	return newlines;
}

/*
 * Since we generate one argument for every parent of a node in the phi
 * function, some arguments may be duplicates of each other. Therefore we
 * must remove them.
 */
void
dm_phi_remove_duplicates(struct phi_function *phi)
{
	int i = 0, j = 0, duplicate = 0;
	int arguments = 0, *indexes = NULL;
	for (i = 0; i < phi->arguments; i++) {
		duplicate = 0;
		for (j = 0; j < phi->arguments; j++)
			if ((phi->indexes[i] == phi->indexes[j]) && (i != j) && (j > i))
				duplicate = 1;
		if (!duplicate) {
			indexes = realloc(indexes, ++arguments * sizeof(int));
			indexes[arguments - 1] = phi->indexes[i];
		}
	}
	free(phi->indexes);
	phi->indexes = indexes;
	phi->arguments = arguments;
}

/*
 * Print an instruction
 */
int
dm_print_ssa_instruction(struct instruction *insn)
{
	struct dm_dwarf_sym_cache_entry *sym = NULL;
	struct dm_cfg_node		*found_node = NULL;
	NADDR				 addr = 0;
	char				*hex = NULL, *temp = NULL;
	int				 colour_set = 0, length = 0;
	/* Translate into ssa assembler */
	dm_translate_intel_ssa(insn);

	if ((insn->ud.br_far) || (insn->ud.br_near) ||
	    (instructions[insn->ud.mnemonic].jump)) {
		/* jumps and calls are yellow */
		printf(ANSII_BROWN);
		colour_set = 1;
	}
	else if ((insn->ud.mnemonic == UD_Iret) ||
	    (insn->ud.mnemonic == UD_Iretf)) {
		/* Returns are red */
		printf(ANSII_RED);
		colour_set = 1;
	}

	length += printf("  ");
	addr = insn->ud.pc - ud_insn_len(&(insn->ud));
	length += printf(NADDR_FMT, addr);
	hex = ud_insn_hex(&(insn->ud));
	/* If possible print target of jumps and calls as a block number or
	 * function name */
	if (instructions[insn->ud.mnemonic].jump ||
	    (insn->ud.mnemonic == UD_Icall))
		addr = dm_get_jump_target(insn->ud);

	if ((instructions[insn->ud.mnemonic].jump) &&
	    (found_node = dm_find_cfg_node_starting(addr))) {
		asprintf(&temp, "%s (Block %d)", insn->ud.insn_buffer, found_node->post);
		length += printf(": %-25s%-40s  ", hex, temp);
		free(temp);
	}
	else if ((insn->ud.mnemonic == UD_Icall) &&
	    (dm_dwarf_find_sym_at_offset(addr, &sym) == DM_OK)) {
		asprintf(&temp, "%s (%s)", insn->ud.insn_buffer, sym->name);
		length += printf(": %-25s%-40s  ", hex, temp);
		free(temp);
	}
	else
		length += printf(": %-25s%-40s  ", hex, insn->ud.insn_buffer);

	/* Set colour back to white if required */
	if (colour_set) {
		printf(ANSII_WHITE);
		colour_set = 0;
	}
	return 0;
}

/*
 * Index all variable uses, build a list of instructions for each block
 */
void
dm_rename_variables(struct dm_cfg_node *n)
{
	struct instruction	*insn = NULL;
	struct ptrs		*p_iter = NULL;
	struct dm_cfg_node	*node = NULL;
	int			 index[3][2] = {{0, 0}, {0, 0}, {0, 0}};
	int			 reg = 0, s_size = 0;
	int			 i = 0, j = 0, k = 0;
	/* For each statement in node n */
	/* Start with phi functions */
	for (i = 0; i < n->pf_count; i++) {
		reg = n->phi_functions[i].var;
		indices[reg].count++;
		dm_ssa_index_stack_push((enum ud_type)reg, indices[reg].count);
		n->phi_functions[i].index =
		    indices[reg].stack[indices[reg].s_size - 1];
	}
	/* Then normal instructions/statements */
	for (dm_seek(n->start); ud.pc - ud_insn_len(&ud) != n->end;) {
		dm_ssa_disassemble(&ud);
		/* For each use of a variable, use the correct index */
		/* Operand 0 */
		if (ud.operand[0].type == UD_OP_MEM) {
			reg = (int)ud.operand[0].base;
			s_size = indices[reg].s_size - 1;
			index[0][0] = indices[reg].stack[s_size];
			reg = (int)ud.operand[0].index;
			s_size = indices[reg].s_size - 1;
			index[0][1] = indices[reg].stack[s_size];
		}
		else
			index[0][0] = index[0][1] = -1;
		/* Operand 1 */
		if (ud.operand[1].type == UD_OP_MEM) {
			reg = (int)ud.operand[1].base;
			s_size = indices[reg].s_size - 1;
			index[1][0] = indices[reg].stack[s_size];
			reg = (int)ud.operand[1].index;
			s_size = indices[reg].s_size - 1;
			index[1][1] = indices[reg].stack[s_size];
		}
		else if (ud.operand[1].type == UD_OP_REG) {
			reg = (int)ud.operand[1].base;
			s_size = indices[reg].s_size - 1;
			index[1][0] = indices[reg].stack[s_size];
			index[1][1] = -1;
		}
		else
			index[1][0] = index[1][1] = -1;
		/* Operand 3 */
		if (ud.operand[2].type == UD_OP_MEM) {
			reg = (int)ud.operand[2].base;
			s_size = indices[reg].s_size - 1;
			index[2][0] = indices[reg].stack[s_size];
			reg = (int)ud.operand[2].index;
			s_size = indices[reg].s_size - 1;
			index[2][1] = indices[reg].stack[s_size];
		}
		else if (ud.operand[2].type == UD_OP_REG) {
			reg = (int)ud.operand[2].base;
			s_size = indices[reg].s_size - 1;
			index[2][0] = indices[reg].stack[s_size];
			index[2][1] = -1;
		}
		/* Is there a definition of a variable? */
		if (instructions[ud.mnemonic].write &&
		    ud.operand[0].type == UD_OP_REG) {
			reg = (int)ud.operand[0].base;
			indices[reg].count++;
			dm_ssa_index_stack_push((enum ud_type)reg,
			    indices[reg].count);
			s_size = indices[reg].s_size - 1;
			index[0][0] = indices[reg].stack[s_size];
			index[0][1] = -1;
		}
		else if (ud.operand[0].type == UD_OP_REG) {
			reg = (int)ud.operand[0].base;
			s_size = indices[reg].s_size - 1;
			index[0][0] = indices[reg].stack[s_size];
			index[0][1] = -1;
		}

		/* Create a new instruction object and store this data */
		insn = malloc(sizeof(struct instruction));
		insn->ud = ud;
		memcpy(insn->index, index, sizeof(index));
		insn->constraints = NULL;
		insn->c_counts = NULL;
		insn->d_count = 0;
		/* Add instruction to block's list */
		n->instructions =
		    realloc(n->instructions, sizeof(void*) * ++n->i_count);
		n->instructions[n->i_count - 1] = insn;
	}
	/* For each child of n */
	for (i = 0; n->children[i] != NULL; i++) {
		for (j = 0; j < n->children[i]->p_count; j++)
			if (n->children[i]->parents[j] == n)
				break;
		node = (struct dm_cfg_node*) n->children[i];
		/* n is the jth parent of child i */
		/* Put the right index on the jth argument of all phi funcs in
		 * child i */
		for (k = 0; k < node->pf_count; k++) {
			reg = node->phi_functions[k].var;
			node->phi_functions[k].indexes[j] =
			    indices[reg].stack[indices[reg].s_size - 1];
		}
	}
	/* Call this function on all children (in dom tree) of this node */
	for (p_iter = p_head; p_iter != NULL; p_iter = p_iter->next) {
		node = (struct dm_cfg_node*)p_iter->ptr;
		if ((node->idom == n) && (node != n))
			dm_rename_variables(node);
	}
	/* Now for every definition of a variable in this node pop the ssa
	 * index that was added */
	for (dm_seek(n->start); ud.pc - ud_insn_len(&ud) != n->end;) {
		ud_disassemble(&ud);
		if (instructions[ud.mnemonic].write &&
		    ud.operand[0].type == UD_OP_REG) {
			reg = (int)ud.operand[0].base;
			dm_ssa_index_stack_pop(reg);
		}
	}
	/* Same for phi functions */
	for (i = 0; i < n->pf_count; i++) {
		reg = n->phi_functions[i].var;
		dm_ssa_index_stack_pop(reg);
	}
}

/*
 * Translate a ud struct (instruction) into ssa assembler
 */
void
dm_translate_intel_ssa(struct instruction *insn)
{
	struct ud*	u = &(insn->ud);
	int		index[3][2];

	memcpy(index, insn->index, sizeof(index));
	/* -- prefixes -- */

	/* check if P_OSO prefix is used */
	if (! P_OSO(u->itab_entry->prefix) && u->pfx_opr) {
		switch (u->dis_mode) {
			case 16:
				mkasm(u, "o32 ");
				break;
			case 32:
			case 64:
				mkasm(u, "o16 ");
				break;
		}
	}

	/* check if P_ASO prefix was used */
	if (! P_ASO(u->itab_entry->prefix) && u->pfx_adr) {
		switch (u->dis_mode) {
			case 16:
				mkasm(u, "a32 ");
				break;
			case 32:
				mkasm(u, "a16 ");
				break;
			case 64:
				mkasm(u, "a32 ");
				break;
		}
	}

	if (u->pfx_seg && u->operand[0].type != UD_OP_MEM &&
	    u->operand[1].type != UD_OP_MEM ) {
	    mkasm(u, "%s", ud_reg_tab[u->pfx_seg - UD_R_AL]);
	}
	if (u->pfx_lock)
		mkasm(u, "lock ");
	if (u->pfx_rep)
		mkasm(u, "rep ");
	if (u->pfx_repne)
		mkasm(u, "repne ");

	/* print the instruction mnemonic */
	mkasm(u, "%s ", ud_lookup_mnemonic(u->mnemonic));

	/* operand 1 */
	if (u->operand[0].type != UD_NONE) {
		int cast = 0;
		if (u->operand[0].type == UD_OP_IMM &&
		    u->operand[1].type == UD_NONE)
			cast = u->c1;
		if (u->operand[0].type == UD_OP_MEM) {
			cast = u->c1;
			if (u->operand[1].type == UD_OP_IMM ||
			    u->operand[1].type == UD_OP_CONST)
				cast = 1;
			if (u->operand[1].type == UD_NONE)
				cast = 1;
			if ((u->operand[0].size != u->operand[1].size ) &&
			    u->operand[1].size)
				cast = 1;
		} else if ( u->operand[ 0 ].type == UD_OP_JIMM )
			if ( u->operand[ 0 ].size > 8 )
				cast = 1;
		insn->cast[0] = cast;
		gen_operand_ssa(u, &u->operand[0], cast, index[0]);
	}
	/* operand 2 */
	if (u->operand[1].type != UD_NONE) {
		int cast = 0;
		mkasm(u, ", ");
		if ( u->operand[1].type == UD_OP_MEM ) {
			cast = u->c1;
			if ( u->operand[0].type != UD_OP_REG )
				cast = 1;
			if ( u->operand[0].size != u->operand[1].size &&
			    u->operand[1].size )
				cast = 1;
			if ( u->operand[0].type == UD_OP_REG &&
			    u->operand[0].base >= UD_R_ES &&
			    u->operand[0].base <= UD_R_GS )
				cast = 0;
		}
		insn->cast[1] = cast;
		gen_operand_ssa(u, &u->operand[1], cast, index[1]);
	}

	/* operand 3 */
	if (u->operand[2].type != UD_NONE) {
		mkasm(u, ", ");
		insn->cast[2] = u->c3;
		gen_operand_ssa(u, &u->operand[2], u->c3, index[2]);
	}
}

/*
 * Translate an operand of a ud struct into ssa assembly form
 */
void
gen_operand_ssa(struct ud* u, struct ud_operand* op, int syn_cast, int *index)
{
	switch(op->type) {
		case UD_OP_REG:
			mkasm(u, "%s_%d", ud_reg_tab[op->base - UD_R_AL],
			    index[0]);
			break;
		case UD_OP_MEM: {
			int op_f = 0;

			if (syn_cast)
				opr_cast(u, op); /* XXX fix warning */

			mkasm(u, "[");

			if (u->pfx_seg)
				mkasm(u, "%s:",
				    ud_reg_tab[u->pfx_seg - UD_R_AL]);

			if (op->base) {
				mkasm(u, "%s_%d",
				    ud_reg_tab[op->base - UD_R_AL], index[0]);
				op_f = 1;
			}

			if (op->index) {
				if (op_f)
					mkasm(u, "+");
				mkasm(u, "%s_%d",
				    ud_reg_tab[op->index -UD_R_AL], index[1]);
				op_f = 1;
			}

			if (op->scale)
				mkasm(u, "*%d", op->scale);

			if (op->offset == 8) {
				if (op->lval.sbyte < 0)
					mkasm(u, "-0x%x", -op->lval.sbyte);
				else
					mkasm(u, "%s0x%x", (op_f) ? "+" : "",
					    op->lval.sbyte);
			}
			else if (op->offset == 16)
				mkasm(u, "%s0x%x", (op_f) ? "+" : "",
				    op->lval.uword);
			else if (op->offset == 32) {
				if (u->adr_mode == 64) {
					if (op->lval.sdword < 0)
						mkasm(u, "-0x%x",
						    -op->lval.sdword);
					else
						mkasm(u, "%s0x%x",
						    (op_f) ? "+" : "",
						    op->lval.sdword);
				}
				else
					mkasm(u, "%s0x%lx", (op_f) ? "+" : "",
					    op->lval.udword);
			}
			else if (op->offset == 64)
				mkasm(u, "%s0x" FMT64 "x", (op_f) ? "+" : "",
				    op->lval.uqword);

			mkasm(u, "]");
			break;
		}

		case UD_OP_IMM: {
			int64_t  imm = 0;
			uint64_t sext_mask = 0xffffffffffffffffull;
			unsigned sext_size = op->size;

			if (syn_cast)
				opr_cast(u, op);
			switch (op->size) {
				case  8: imm = op->lval.sbyte; break;
				case 16: imm = op->lval.sword; break;
				case 32: imm = op->lval.sdword; break;
				case 64: imm = op->lval.sqword; break;
			}
			if ( P_SEXT( u->itab_entry->prefix ) ) {
				sext_size = u->operand[ 0 ].size;
				if ( u->mnemonic == UD_Ipush )
					/* push sign-extends to operand size */
					sext_size = u->opr_mode;
			}
			if ( sext_size < 64 )
				sext_mask = ( 1ull << sext_size ) - 1;
			mkasm( u, "0x" FMT64 "x", imm & sext_mask );

			break;
		}

		case UD_OP_JIMM:
			if (syn_cast) opr_cast(u, op);
			switch (op->size) {
				case  8:
					mkasm(u, "0x" FMT64 "x", u->pc +
					    op->lval.sbyte);
					break;
				case 16:
					mkasm(u, "0x" FMT64 "x", ( u->pc +
					    op->lval.sword ) & 0xffff );
					break;
				case 32:
					mkasm(u, "0x" FMT64 "x", ( u->pc +
					    op->lval.sdword ) & 0xfffffffful );
					break;
				default:break;
			}
			break;

		case UD_OP_PTR:
			switch (op->size) {
				case 32:
					mkasm(u, "word 0x%x:0x%x",
					    op->lval.ptr.seg,
					    op->lval.ptr.off & 0xFFFF);
					break;
				case 48:
					mkasm(u, "dword 0x%x:0x%lx",
					    op->lval.ptr.seg,
					    op->lval.ptr.off);
					break;
			}
			break;

		case UD_OP_CONST:
			if (syn_cast) opr_cast(u, op);
			mkasm(u, "%d", op->lval.udword);
			break;

		default: return;
	}
}

/*
 * Place phi functions in all the correct nodes
 */
void
dm_place_phi_functions()
{
	struct dm_cfg_node	**W = NULL, *n = NULL, *dn = NULL;//, **B = NULL;
	unsigned int		  i = 0;
	int			  j = 0, k = 0;
	int			  w_size = 0, duplicate = 0;//, b_size = 0;

	/* For each variable */
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		/* Build a worklist W */
		free(W);
		W = malloc(indices[i].dn_count * sizeof(void*));
		for (j = 0; j < indices[i].dn_count; j++)
			W[j] = indices[i].def_nodes[j];
		w_size = indices[i].dn_count;
		/* While the worklist is not empty */
		while (w_size) {
			/* Find a node n that hasn't already been checked */
			/*while (w_size &&
			    dm_array_contains(B, b_size, W[w_size -1]))
				w_size--;*/
			/*if (!w_size)
				break;*/
			/* Remove node n from W */
			n = W[w_size - 1];
			W = realloc(W, --w_size * sizeof(void*));
			/* Add n to blacklist so we dont check it twice or get
			 * stuck in an infinite loop */
			/*B = realloc(B, ++b_size * sizeof(void*));
			B[b_size - 1] = n;*/
			/* For each node dn in DF of n */
			for (j = 0; j < n->df_count; j++) {
				dn = (struct dm_cfg_node*)n->df_set[j];
				/* Note in i that i has a phi node in dn */
				if (!dm_array_contains(indices[i].phi_nodes,
				    indices[i].pn_count, dn)) {
					indices[i].phi_nodes =
					    realloc(indices[i].phi_nodes,
					    ++indices[i].pn_count *
					    sizeof(void*));
					indices[i].phi_nodes
					    [indices[i].pn_count -1] = dn;
				}
				duplicate = 0;
				/* Put phi functions in block dn */
				for (k = 0; k < dn->pf_count; k++)
					if (dn->phi_functions[k].var == i) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
					dn->phi_functions =
					    realloc(dn->phi_functions,
					    ++dn->pf_count * sizeof(struct phi_function));
					dn->phi_functions[dn->pf_count - 1].var = i;
					dn->phi_functions[dn->pf_count - 1].arguments = dn->p_count;
					dn->phi_functions[dn->pf_count - 1].indexes = malloc(dn->p_count * sizeof(int));
					dn->phi_functions[dn->pf_count - 1].index = 0;
					dn->phi_functions[dn->pf_count - 1].constraints = NULL;
					dn->phi_functions[dn->pf_count - 1].c_counts = NULL;
					dn->phi_functions[dn->pf_count - 1].d_count = 0;
				}
				/* Add dn to worklist */
				if (dn != n) { //!dm_array_contains(W, w_size, dn)) {
					W = realloc(W, ++w_size *
					    sizeof(void*));
					W[w_size - 1] = dn;
				}
			}
		}
	}
	//free(B);
	free(W);
}

/*
 * Returns 1 if an array contains a specific node, 0 otherwise
 */
int
dm_array_contains(struct dm_cfg_node **list, int c, struct dm_cfg_node *term)
{
	int i = 0;
	for (i = 0; i < c; i++) {
		if (list[i] == term)
			return 1;
	}
	return 0;
}

/*
 * Find all definitions of all vairables
 */
void
dm_ssa_find_var_defs()
{
	struct dm_cfg_node	*n = NULL;
	unsigned int		 read = 0;
	enum ud_type		 reg = 0;
	int			 duplicate = 0, i =0;

	/* For all nodes n */
	for (p = p_head; p != NULL; p = p->next) {
		n = (struct dm_cfg_node*)p->ptr;
		/* For all statements in node n */
		for (dm_seek(n->start); ud.pc - ud_insn_len(&ud) != n->end;) {
			read = ud_disassemble(&ud);
			//n->s_count++;
			/* If instruction writes to a register */
			if ((instructions[ud.mnemonic].write)
			    && (ud.operand[0].type == UD_OP_REG)) {
				reg = ud.operand[0].base;
				/* Record that n contains definition of reg */
				if (!dm_array_contains(indices[reg].def_nodes,
				    indices[reg].dn_count, n)) {
					indices[reg].def_nodes
					    = realloc(indices[reg].def_nodes,
					    ++indices[reg].dn_count
					    * sizeof(void*));
					indices[reg].def_nodes
					    [indices[reg].dn_count -1] = n;
				}
				duplicate = 0;
				for (i = 0; i < n->dv_count; i++)
					if (n->def_vars[i] == reg) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
					n->def_vars = realloc(n->def_vars,
					    ++n->dv_count * sizeof(int));
					n->def_vars[n->dv_count -1] = reg;
				}
			}
		}
	}
}

/*
 * Push an index onto the stack for a register
 */
void
dm_ssa_index_stack_push(enum ud_type reg, int i)
{
	indices[reg].stack = realloc(indices[reg].stack,
	    (++indices[reg].s_size) * sizeof(int));
	indices[reg].stack[indices[reg].s_size - 1] = i;
}

/*
 * Pop an index from a reisters stack
 */
int
dm_ssa_index_stack_pop(enum ud_type reg)
{
	if (!indices[reg].s_size) {
		printf("Tried to pop empty stack (reg %s %d)!\n",
		    ud_reg_tab[reg - 1], reg);
		return -1;
	}
	int i = indices[reg].stack[indices[reg].s_size - 1];
	indices[reg].stack = realloc(indices[reg].stack,
	    (--indices[reg].s_size) * sizeof(int));
	return i;
}

/*
 * Initialise the register indexing struct array
 */
void
dm_ssa_index_init()
{
	int	i;

	indices = malloc(sizeof(struct dm_ssa_index) * (UD_OP_CONST + 1));

	/* Initialise struct for SSA indexes */
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		indices[i].reg = (enum ud_type)i;
		indices[i].count = 0;
		indices[i].stack = malloc(sizeof(int));
		indices[i].stack[0] = 0;
		indices[i].s_size = 1;
		indices[i].def_nodes = NULL;
		indices[i].dn_count = 0;
		indices[i].phi_nodes = NULL;
		indices[i].pn_count = 0;
	}
}

void
dm_free_ssa()
{
	struct dm_cfg_node	*node = NULL;
	int			 i = 0;

	for (p = p_head; p != NULL; p = p->next) {
		node = (struct dm_cfg_node*)p->ptr;
		free(node->def_vars);
		for (i = 0; i < node->pf_count; i++) {
			free(node->phi_functions[i].indexes);
		}
		free(node->phi_functions);
		for (i = 0; i < node->i_count; i ++) {
			free(node->instructions[i]);
		}
		free(node->instructions);
	}
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		free(indices[i].stack);
		free(indices[i].def_nodes);
		free(indices[i].phi_nodes);
	}
	free(indices);
}

