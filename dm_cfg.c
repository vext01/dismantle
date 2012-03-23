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
#include <stdio.h>
#include "dm_cfg.h"
#include "dm_gviz.h"
#include "dm_dwarf.h"

/* Head of the list and a list iterator */
struct ptrs	*p_head = NULL;
struct ptrs	*p = NULL;
struct ptrs	*p_iter = NULL;

/* Explicitly record the list length */
int		p_length = 0;

struct dm_instruction_se *instructions = NULL;

void **rpost; /* Pointers to nodes in reverse post-order */

int fcalls_i = 0;

/*
 * Generate static CFG for a function.
 * Continues until it reaches ret, does not follow calls.
 */
int
dm_cmd_cfg(char **args) {
	struct	dm_cfg_node *cfg = NULL;

	(void) args;

	/* Initialise structures */
	dm_init_cfg();

	/* Get CFG */
	cfg = dm_recover_cfg();

	/* Graph CFG */
	dm_graph_cfg();

	/* Print CFG */
	dm_print_cfg(cfg);

	/* Check CFG for consistency! */
	dm_check_cfg_consistency();

	/* Free all memory */
	dm_free_cfg();

	return (0);
}

/*
 * This function actually starts the building process
 * Returns completed CFG
 */
struct dm_cfg_node*
dm_recover_cfg() {
	NADDR	addr = cur_addr;
	struct	dm_cfg_node *cfg = NULL;

	/* Create first node */
	cfg = dm_new_cfg_node(addr, 0);

	/* Create CFG */
	dm_gen_cfg_block(cfg);

	/* Get reverse postorder, preorder and postorder of nodes */
        rpost = calloc(p_length, sizeof(void*));
        dm_depth_first_walk(cfg);

	/* Rewind back */
	dm_seek(addr);

	return cfg;
}

void
dm_check_cfg_consistency()
{
	struct dm_cfg_node *node = NULL;
	int i = 0, j = 0, consistent = 0;
	for (p = p_head; p != NULL; p = p->next) {
		node = (struct dm_cfg_node*)p->ptr;
		for (i = 0; node->children[i] != NULL; i++) {
			consistent = 0;
			for (j = 0; j < node->children[i]->p_count; j++) {
				if (node->children[i]->parents[j] == node)
					consistent = 1;
			}
			if (!consistent)
				printf("No link from node %d (start addr "NADDR_FMT ") to parent %d!\n",
				    node->children[i]->post, node->children[i]->start, node->post);
		}
		for (i = 0; i < node->p_count; i++) {
			consistent = 0;
			for (j = 0; node->parents[i]->children[j] != NULL; j++) {
				if (node->parents[i]->children[j] == node)
					consistent = 1;
			}
			if (!consistent)
				printf("No link from node %d (start addr "NADDR_FMT") to child %d (start addr "NADDR_FMT")!\n",
				    node->parents[i]->post, node->parents[i]->start, node->post, node->start);
		}
	}
}

/*
 * Initialise structures used for CFG recovery
 */
void
dm_init_cfg()
{
	struct	dm_setting *fcalls = NULL;

	/* Get fcalls setting */
	dm_find_setting("cfg.fcalls", &fcalls);
	fcalls_i = fcalls->val.ival;

	dm_instruction_se_init();
}

/*
 * We create an array of structures indicating semantics of each instruction
 */
/* nasty hack, overapproximates size of ud enum in itab.h, fix XXX */
#define DM_UD_ENUM_HACK				600
void
dm_instruction_se_init()
{
	int c;

	instructions =
	    malloc(sizeof(struct dm_instruction_se) * (DM_UD_ENUM_HACK));

	/* Initialise struct recording which instructions write to registers */
	for (c = 0; c < DM_UD_ENUM_HACK; c++) {
		instructions[c].instruction = c;
		instructions[c].write = 1;
		instructions[c].jump = 0;
		instructions[c].ret = 0;
		instructions[c].disjunctive = 0;
	}

	/* XXX store in linked list (queue.h) */
	instructions[UD_Ipush].write = 0;
	instructions[UD_Itest].write = 0;

	instructions[UD_Iret].write = 0;
	instructions[UD_Iret].ret = 1;

	instructions[UD_Ijmp].write = 0;
	instructions[UD_Ijmp].jump = 1;

	instructions[UD_Ijz].write = 0;
	instructions[UD_Ijz].jump = 2;

	instructions[UD_Ijnz].write = 0;
	instructions[UD_Ijnz].jump = 2;

	instructions[UD_Icmp].write = 0;

	instructions[UD_Ijg].write = 0;
	instructions[UD_Ijg].jump = 2;

	instructions[UD_Ijae].write = 0;
	instructions[UD_Ijae].jump = 2;

	instructions[UD_Ijle].write = 0;
	instructions[UD_Ijle].jump = 2;

	instructions[UD_Ijl].write = 0;
	instructions[UD_Ijl].jump = 2;

	instructions[UD_Ija].write = 0;
	instructions[UD_Ija].jump = 2;

	instructions[UD_Ijb].write = 0;
	instructions[UD_Ijb].jump = 2;

	instructions[UD_Ijbe].write = 0;
	instructions[UD_Ijbe].jump = 2;

	instructions[UD_Ijcxz].write = 0;
        instructions[UD_Ijcxz].jump = 2;

	instructions[UD_Ijnp].write = 0;
	instructions[UD_Ijnp].jump = 2;

	instructions[UD_Ijge].write = 0;
	instructions[UD_Ijge].jump = 2;

	instructions[UD_Icall].write = 0;
	if (fcalls_i)
		instructions[UD_Icall].jump = 2;

	instructions[UD_Iadd].disjunctive = 1;

	instructions[UD_Isub].disjunctive = 1;
}

/*
 * Create a new node in the CFG
 */
struct dm_cfg_node *
dm_new_cfg_node(NADDR nstart, NADDR nend)
{
	struct dm_cfg_node		*node;

	node = malloc(sizeof(struct dm_cfg_node));
	node->start = nstart;
	node->end = nend;
	node->children = calloc(1, sizeof(void*));
	node->c_count = 0;
	node->parents = NULL;
	node->p_count = 0;
	node->nonlocal = 0;
	node->visited = 0;
	node->pre = 0;
	node->rpost = 0;
	node->idom = NULL;
	node->df_set = NULL;
	node->df_count = 0;
	node->def_vars = NULL;
	node->dv_count = 0;
	node->phi_functions = NULL;
	node->pf_count = 0;
	node->instructions = NULL;
	node->i_count = 0;
	/* Add node to the free list so we can free the memory at the end */
	if (p) {
		p->next = calloc(1, sizeof(struct ptrs));
		p = p->next;
		p->ptr = (void*)node;
	}
	else {
		p = calloc(1, sizeof(struct ptrs));
		p->ptr = (void*)node;
		p_head = p;
	}
	p_length++;

	return (node);
}

void
dm_add_parent(struct dm_cfg_node *node, struct dm_cfg_node *parent)
{
	node->parents = realloc(node->parents, ++(node->p_count) *
	    sizeof(void*));
	node->parents[node->p_count - 1] = parent;
}

/*
 * Main part of CFG recovery. Recursively find blocks.
 */
struct dm_cfg_node *
dm_gen_cfg_block(struct dm_cfg_node *node)
{
	NADDR			 addr = node->start;
	unsigned int		 read = 0, oldRead = 0;
	char			*hex;
	struct dm_cfg_node	*foundNode = NULL;
	NADDR			 target = 0;
	int			 i = 0, duplicate = 0, local_target = 1;

	dm_seek(node->start);
	while (1) {
		oldRead = read;
		read = ud_disassemble(&ud);
		hex = ud_insn_hex(&ud);

		/* Check we haven't run into the start of another block */
		if ((foundNode = dm_find_cfg_node_starting(addr))
		    && (foundNode != node)) {
			addr -= oldRead;
			free(node->children);
			node->children = calloc(2, sizeof(void*));
			node->children[0] = foundNode;
			dm_add_parent(foundNode, node);
			break;
		}

		/*
		 * Check for jump instructions and create
		 * new nodes as necessary
		 *
		 * Make sure the target is inside the .text
		 * section */
		local_target = 1;
		if (instructions[ud.mnemonic].jump) {
			target = dm_get_jump_target(ud);
			if (!dm_is_target_in_text(target))
				local_target = 0;
		}

		if (instructions[ud.mnemonic].jump && (local_target ||
		    ((!local_target) && (fcalls_i == 2)))) {
			/* Get the target of the jump instruction */
			target = dm_get_jump_target(ud);

			/* End the block here */
			node->end = addr;
			free(node->children);

			/* Make space for the children of this block */
			node->children = calloc(instructions[ud.mnemonic].jump
			    + 1, sizeof(void*));

			/* Check if we are jumping to the start of an already
			 * existing block, if so use that as child of current
			 * block */
			if (((foundNode = dm_find_cfg_node_starting(target))
			    != NULL) && local_target) {
				node->children[0] = foundNode;
				dm_add_parent(foundNode, node);
			}
			/* Check if we are jumping to the *middle* of an
			 * existing block, if so split it and use 2nd half as
			 * child of current block */
			else if (((foundNode = dm_find_cfg_node_containing(
			    target)) != NULL) && local_target) {
				/* We found a matching block. Now find address
				 * before addr and split the block */
				node->children[0] = dm_split_cfg_block(
				    foundNode, target);

				duplicate = 0;
				for (i = 0; i < node->children[0]->p_count; i++)
					if (node->children[0]->parents[i] == node)
						duplicate = 1;
				/* Node is recursive, make it it's own parent */
				if (duplicate)
					dm_add_parent(node->children[0],
					    node->children[0]);
				else
					dm_add_parent(node->children[0], node);
			}
			/* This is a new block, so scan with a recursive call
			 * to find it's start, end, and children, assuming it's
			 * a local block (inside the binary) */
			else if (local_target) {
				node->children[0] = dm_new_cfg_node(target, 0);
				dm_add_parent(node->children[0], node);
				dm_gen_cfg_block(node->children[0]);
			}
			/* This target is outside of the binary. Just make a
			 * basic block for it with and continue with a new
			 * block from the next insn */
			if (!local_target) {
				if ((foundNode = dm_find_cfg_node_starting(target)) != NULL) {
					dm_add_parent(foundNode, node);
					node->children[0] = foundNode;
				}
				else {
					/* New block starts and ends at target addr */
					node->children[0] = dm_new_cfg_node(target, target);
					dm_add_parent(node->children[0], node);
					node->children[0]->nonlocal = 1;
				}

				/* New node has child starting at next insn */
				dm_seek(addr);
				read = ud_disassemble(&ud);

				node->children[0]->children =
				    realloc(node->children[0]->children, (1 + ++(node->children[0]->c_count))*sizeof(void*));
				node->children[0]->children[node->children[0]->c_count-1] =
				    dm_new_cfg_node(ud.pc, 0);
				node->children[0]->children[node->children[0]->c_count] = NULL;
				dm_add_parent(node->children[0]->children[node->children[0]->c_count-1], node->children[0]);
				dm_gen_cfg_block(node->children[0]->children[node->children[0]->c_count-1]);
			}
			else {
				/* Seek back to before we followed the jump */
				dm_seek(addr);
				read = ud_disassemble(&ud);
			}
			/* Check whether there was some sneaky splitting of the
			 * block we're working on while we were away! */
			if (node->end < addr) {
				/* Now we must find the right block to continue
				 * from */
				foundNode = dm_find_cfg_node_ending(addr);
				if (foundNode != NULL) {
					node = foundNode;
				}
			}

			/*
			 * If the jump was a conditional, now we must
			 * follow the other leg of the jump
			 */
			if (instructions[ud.mnemonic].jump > 1) {
				if ((node->children[1] =
				    dm_find_cfg_node_starting(ud.pc)) != NULL) {
					dm_add_parent(node->children[1], node);
					break;
				}
				else {
					node->children[1] =
					    dm_new_cfg_node(ud.pc, 0);
					dm_add_parent(node->children[1], node);
					node = node->children[1];
				}
			}
			else
				break;
		}
		/* If we find a return end the block/node */
		if (instructions[ud.mnemonic].ret)
			break;
		addr += read;
	}
	node->end = addr;
	return node;
}

int
dm_is_target_in_text(NADDR addr)
{
	NADDR		start = 0, size = 0;
	GElf_Shdr	shdr;

	if ((dm_find_section(".text", &shdr)) == DM_FAIL) {
		return (0);
	}

	start = shdr.sh_offset;
	size = shdr.sh_size;

	if ((addr < start) || (addr > (start + size)))
		return (0);

	return (1);
}

struct dm_cfg_node *
dm_split_cfg_block(struct dm_cfg_node *node, NADDR addr)
{
	struct dm_cfg_node *tail = NULL;
	NADDR addr2 = node->start;
	unsigned int read = 0;
	int i = 0, j = 0;

	/* Tail node runs from split address to end of original node */
	tail = dm_new_cfg_node(addr, node->end);
	free(tail->children);

	/* Tail node must pick up original nodes children */
	tail->children = node->children;

	/* First parent of tail node is the head node */
	dm_add_parent(tail, node);

	/* Find address of instruction before the split (end of head node) */
	for (dm_seek(node->start); addr2 + read < addr; addr2 += read)
		read = ud_disassemble(&ud);

	node->end = addr2;

	/* Head has only one child - the tail node */
	node->children = calloc(2, sizeof(void*));
	node->children[0] = tail;

	/* We must find all children of the original node and change the
	 * parents entry that pointed to the original node to point to the
	 * new tail node */
	for (i = 0; tail->children[i] != NULL; i++)
		for (j = 0; j < tail->children[i]->p_count; j++)
			if (tail->children[i]->parents[j] == node) {
				tail->children[i]->parents[j] = tail;
			}
	/* Finally, return the new tail node*/
	return tail;
}

/*
 * Searches all blocks for one starting with addr, and returns it if found
 * (otherwise returns NULL)
 */
struct dm_cfg_node *
dm_find_cfg_node_starting(NADDR addr)
{
	struct dm_cfg_node	*node;

	for (p_iter = p_head;
	    (p_iter != NULL); p_iter = p_iter->next) {
		if (p_iter->ptr != NULL) {
			node = (struct dm_cfg_node*)(p_iter->ptr);
			if (node->start == addr)
				return node;
		}
	}

	return (NULL);
}

/*
 * Searches all blocks for one ending with addr, and returns it if found
 * (otherwise returns NULL)
 */
struct dm_cfg_node *
dm_find_cfg_node_ending(NADDR addr)
{
	struct dm_cfg_node	*node;

	for (p_iter = p_head; p_iter != NULL; p_iter = p_iter->next) {
		node = (struct dm_cfg_node*)(p_iter->ptr);
		if (node->end == addr)
			return (node);
	}
	return (NULL);
}

/*
 * Searches all blocks to see if one contains addr and returns it if found
 * (otherwise returns NULL)
 */
struct dm_cfg_node *
dm_find_cfg_node_containing(NADDR addr)
{
	struct dm_cfg_node		*node;

	for (p_iter = p_head; p_iter != NULL; p_iter = p_iter->next) {

		node = (struct dm_cfg_node*) (p_iter->ptr);

		if ((node->start < addr) &&
		    (node->end != 0) && (node->end > addr)) {
			return node;
		}
	}
	return (NULL);
}

/*
 * Use the free list to print info on all the blocks we have found
 */
void
dm_print_cfg()
{
	struct dm_cfg_node	*node;
	int			c;

	for (p = p_head; p != NULL; p = p->next) {
		node = (struct dm_cfg_node*) (p->ptr);

		printf("Block %d start: " NADDR_FMT ", end: " NADDR_FMT
		    "\n", node->post, node->start, node->end);

		if (node->children[0] != NULL) {
			printf("\tChild blocks: ");
			for (c = 0; node->children[c] != NULL; c++)
				printf("%d ", node->children[c]->post);
			printf("\n");
		}

		if (node->p_count) {
			printf("\tParent blocks: ");
			for (c = 0; c < node->p_count; c++)
				printf("%d ", node->parents[c]->post);
			printf("\n");
		}
	}
}

/*
 * Free all data structures used for building the CFG
 */
void
dm_free_cfg()
{
	struct ptrs *p_prev = NULL;

	p = p_head;
	while (p != NULL) {
		if (p->ptr != NULL) {
			free(((struct dm_cfg_node*)(p->ptr))->children);
			free(((struct dm_cfg_node*)(p->ptr))->parents);
		}
		free(p->ptr);
		p_prev = p;
		p = p->next;
		free(p_prev);
	}
	free(instructions);
	free(rpost);
	p_length = 0;
}

/*
 * Do a depth-first walk of the CFG to get the reverse post-order
 * (and post-order and pre-order) of the nodes
 */
int i, j;

void
dm_depth_first_walk(struct dm_cfg_node *cfg)
{
	struct dm_cfg_node *node = cfg;
	i = 0;
	j = p_length - 1;
	p = p_head;
	while ((node = dm_get_unvisited_node(p)))
		dm_dfw(node);
}

void
dm_dfw(struct dm_cfg_node *node)
{
	int c = 0;
	node->visited = 1;
	node->pre = i++;
	for (;node->children[c] != NULL; c++)
		if (!node->children[c]->visited)
			dm_dfw(node->children[c]);
	rpost[j] = node;
	node->rpost = j--;
	node->post = p_length - 1 - node->rpost;
}

struct dm_cfg_node*
dm_get_unvisited_node()
{
	for (p = p_head; p != NULL; p = p->next) {
		if (!((struct dm_cfg_node*)(p->ptr))->visited)
			return p->ptr;
	}
        return NULL;
}

void
dm_graph_cfg()
{
	/* struct dm_dwarf_sym_cache_entry *sym = NULL; */
        struct dm_cfg_node *node = NULL;
        FILE *fp = dm_new_graph("cfg.dot");
        char *itoa1 = NULL, *itoa2 = NULL;
        int c = 0;

	if (!fp) return;

	for (p = p_head; p != NULL; p = p->next) {
		node = (struct dm_cfg_node*)(p->ptr);

		asprintf(&itoa1, "%d", node->post);
		/*if (dm_dwarf_find_sym_at_offset(node->start, &sym) == DM_OK) {
			asprintf(&itoa2, "%d (%s)\\nstart: " NADDR_FMT "\\nend: "
			    NADDR_FMT, node->post, sym->name, node->start,
			    node->end);
			dm_colour_label(fp, itoa1, "lightpink");
		}
		else {
			asprintf(&itoa2, "%d\\nstart: " NADDR_FMT "\\nend: "
			    NADDR_FMT, node->post, node->start,
			    node->end);
		}
		dm_add_label(fp, itoa1, itoa2);
		free(itoa2);*/

		for (c = 0; node->children[c] != NULL; c++) {
			asprintf(&itoa2, "%d",
			    node->children[c]->post);
			dm_add_edge(fp, itoa1, itoa2);
			if (node->children[c]->post > node->post)
				dm_colour_label(fp, itoa2, "lightblue");
			if (node->nonlocal)
				dm_colour_label(fp, itoa1, "lightpink");
			free(itoa2);
		}
		free(itoa1);
	}
	dm_end_graph(fp);
	dm_display_graph("cfg.dot");
}

