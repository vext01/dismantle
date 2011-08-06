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

#include "dm_ssa.h"

extern struct dm_instruction_se *instructions;
extern struct ptrs		*p_head;
extern struct ptrs		*p;
extern int			 p_length;

struct dm_ssa_index		*indices = NULL;

int
dm_cmd_ssa(char **args)
{
	struct dm_cfg_node      *cfg = NULL;
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

        /* Display dominator tree */
        dm_graph_dom();

        /* Free dominance frontier sets */
        dm_dom_frontiers_free();

        /* Free all CFG structures */
        dm_free_cfg();
	/*struct		dm_ssa_index  *indices = NULL;
	unsigned int	read;
	char		*hex;
	int		ops = strtoll(args[0], NULL, 0), i;
        NADDR		addr = cur_addr;

	dm_ssa_index_init(&indices);
	dm_instruction_se_init();

	printf("\n");
	for (i = 0; i < ops; i++) {
		read = ud_disassemble(&ud);
		hex = ud_insn_hex(&ud);

		printf("    " NADDR_FMT ":  %-20s%s  ",
		    addr, hex, ud_insn_asm(&ud));

		addr = addr + read;

		if (instructions[ud.mnemonic].write)
			indices[ud.operand[0].base].index++;

		printf("%d, ", indices[ud.operand[0].base].index);
		printf("%d, ", indices[ud.operand[1].base].index);
		printf("%d\n", indices[ud.operand[2].base].index);
	}
	*/
	/*dm_seek(cur_addr);*/ /* rewind back */
	/*printf("\n");*/
	return (0);
}

void
dm_place_phi_functions()
{
	struct dm_cfg_node	**W, *n, *dn;
	unsigned int		  i = 0;
	int			  j = 0, k = 0;
	int			  w_size = 0, duplicate = 0;
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		W = malloc(indices[i].dn_count * sizeof(void*));
		W = memcpy(indices[i].def_nodes, W, indices[i].dn_count *
		    sizeof(void*));
		w_size = indices[i].dn_count;
		while (w_size) {
			n = W[w_size - 1];
			W = realloc(W, --w_size * sizeof(void*));
			for (j = 0; j < n->df_count; j++) {
				dn = (struct dm_cfg_node*)n->df_set[j];
				duplicate = 0;
				for (k = 0; k < indices[i].pn_count; k++)
					if (indices[i].phi_nodes[k] == dn) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
					indices[i].phi_nodes =
					    realloc(indices[i].phi_nodes,
					    ++indices[i].pn_count *
					    sizeof(void*));
					indices[i].phi_nodes
					    [indices[i].pn_count -1] = dn;
				}
				duplicate = 0;
				for (k = 0; k < dn->pv_count; k++)
					if (dn->phi_vars[k] == i) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
					dn->phi_vars = realloc(dn->phi_vars,
					    ++dn->pv_count * sizeof(int));
					dn->phi_vars[dn->pv_count - 1] = i;
				}
				duplicate = 0;
				for (k = 0; k < w_size; k++)
					if (W[k] == dn) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
					W = realloc(W, ++w_size *
					    sizeof(void*));
					W[w_size - 1] = dn;
				}
			}
		}
	}
}

//void
//dm_array_contains(void* list, int count, void term

void
dm_ssa_find_var_defs()
{
	struct dm_cfg_node	*n = NULL;
	unsigned int		 read = 0;
	enum ud_type		 reg = 0;
	int			 duplicate = 0, i =0;

	for (p = p_head; p->ptr != NULL; p = p->next) {
		n = (struct dm_cfg_node*)p->ptr;
		for (dm_seek(n->start); ud.pc != n->end;
		    read = ud_disassemble(&ud)) {
			if (instructions[ud.mnemonic].write) {
				reg = ud.operand[0].base;
				duplicate = 0;
				for (i = 0; i < indices[reg].dn_count; i++)
					if (indices[reg].def_nodes[i] == n) {
						duplicate = 1;
						break;
					}
				if (!duplicate) {
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

void
dm_ssa_index_init()
{
	int	i;

	indices = malloc(sizeof(struct dm_ssa_index) * (UD_OP_CONST + 1));

	/* Initialise struct for SSA indexes */
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		indices[i].reg = (enum ud_type)i;
		indices[i].index = 0;
		indices[i].def_nodes = NULL;
		indices[i].dn_count = 0;
		indices[i].phi_nodes = NULL;
		indices[i].pn_count = 0;
	}
}

