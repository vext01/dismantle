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

/* Head of the list and a list iterator */
struct ptrs	*p_head = NULL;
struct ptrs	*p = NULL;

/* Explicitly record the list length */
int		p_length = 0;

struct dm_instruction_se *instructions = NULL;

void **rpost; /* Pointers to nodes in reverse post-order */

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

/*
 * Initialise structures used for CFG recovery
 */
void
dm_init_cfg()
{
	dm_instruction_se_init();

	/* Initialise free list */
	p_head = calloc(1, sizeof(struct ptrs));
	p = p_head;
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

	instructions[UD_Ijle].write = 0;
	instructions[UD_Ijle].jump = 2;

	instructions[UD_Icall].write = 0;
	instructions[UD_Icall].jump = 0;
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
	node->parents = NULL;
	node->p_count = 0;
	node->visited = 0;
	node->pre = 0;
	node->rpost = 0;
	node->idom = NULL;

	/* Add node to the free list so we can free the memory at the end */
	p->ptr = (void*)node;
	p->next = calloc(1, sizeof(struct ptrs));
	p = p->next;

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
	NADDR			addr = node->start;
	unsigned int		read = 0, oldRead = 0;
	char			*hex;
	struct dm_cfg_node	*foundNode = NULL;

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
		 */
		if (instructions[ud.mnemonic].jump) {
			/* End the block here */
			node->end = addr;
			free(node->children);

			/* Make space for the children of this block */
			node->children = calloc(instructions[ud.mnemonic].jump
			    + 1, sizeof(void*));

			/* Check if we are jumping to the start of an already
			 * existing block, if so use that as child of current
			 * block */
			if ((foundNode = dm_find_cfg_node_starting(
			   ud.operand[0].lval.sdword + addr + read)) != NULL) {
				node->children[0] = foundNode;
				dm_add_parent(foundNode, node);
			}
			/* Check if we are jumping to the *middle* of an
			 * existing block, if so split it and use 2nd half as
			 * child of current block */
			else if ((foundNode = dm_find_cfg_node_containing(
			   ud.operand[0].lval.sdword + addr + read)) != NULL) {
				node->children[0] = foundNode;
				dm_add_parent(foundNode, node);
			}
			/* This is a new block, so scan with a recursive call
			 * to find it's start, end, and children */
			else {
				node->children[0] = dm_new_cfg_node(
				   ud.operand[0].lval.sdword + addr + read, 0);
				dm_add_parent(node->children[0], node);
				dm_gen_cfg_block(node->children[0]);
			}

			/* Seek back to before we followed the jump */
			dm_seek(addr);
			read = ud_disassemble(&ud);

			/*
			 * If the jump was a conditional, now we must
			 * follow the other leg of the jump
			 */
			if (instructions[ud.mnemonic].jump > 1) {
				if ((node->children[1] =
				    dm_find_cfg_node_starting(addr + read))
				    != NULL) {
					dm_add_parent(node->children[1], node);
					break;
				}
				else {
					node->children[1] =
					    dm_new_cfg_node(addr + read, 0);
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

/*
 * Searches all blocks for one starting with addr, and returns it if found
 * (otherwise returns NULL)
 */
struct dm_cfg_node *
dm_find_cfg_node_starting(NADDR addr)
{
	struct dm_cfg_node		*node;

	for (p = p_head; p->ptr != NULL; p = p->next) {
		node = (struct dm_cfg_node*)(p->ptr);
		if (node->start == addr)
			return node;
	}

	return (NULL);
}

/*
 * Searches all blocks to see if one contains addr. If so,
 * split the block and return the tail (otherwise returns NULL)
 */
struct dm_cfg_node *
dm_find_cfg_node_containing(NADDR addr)
{
	struct dm_cfg_node		*node, *newNode;
	NADDR				addr2;
	unsigned int			read = 0;

	for (p = p_head; p->ptr != NULL; p = p->next) {

		node = (struct dm_cfg_node*) (p->ptr);

		if ((node->start > addr) &&
		    (node->end != 0) && (node->end < addr)) {

			/*
			 * We found a matching block. Now find address before
			 * addr and split the block, return tail block
			 */
			for (dm_seek(node->start);
			    addr2 + read != addr; addr2 += read)
				read = ud_disassemble(&ud);

			newNode = dm_new_cfg_node(addr, node->end);
			newNode->children = node->children;
			dm_add_parent(newNode, node);

			node->children = calloc(2, sizeof(void*));
			node->children[0] = newNode;
			node->end = addr2;

			return (newNode);
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

	for (p = p_head; p->ptr != NULL; p = p->next) {
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
		if (p->ptr != NULL)
			free(((struct dm_cfg_node*)(p->ptr))->children);
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
	p = p_head;
	for (;p->next != NULL; p = p->next)
		if (!((struct dm_cfg_node*)(p->ptr))->visited)
			return p->ptr;
        return NULL;
}

void
dm_graph_cfg()
{
        struct dm_cfg_node *node = NULL;
        FILE *fp = dm_new_graph("cfg.dot");
        char *itoa1 = NULL, *itoa2 = NULL;
        int c = 0;

	if (!fp) return;

	for (p = p_head; p->ptr != NULL; p = p->next) {
		node = (struct dm_cfg_node*)(p->ptr);

		asprintf(&itoa1, "%d", node->post);
		asprintf(&itoa2, "%d\\nstart: " NADDR_FMT "\\nend: "
		    NADDR_FMT, node->post, node->start,
		    node->end);
		dm_add_label(fp, itoa1, itoa2);
		free(itoa2);

		for (c = 0; node->children[c] != NULL; c++) {
			asprintf(&itoa2, "%d",
			    node->children[c]->post);
			dm_add_edge(fp, itoa1, itoa2);
			free(itoa2);
		}
		free(itoa1);
	}
	dm_end_graph(fp);
	dm_display_graph("cfg.dot");
}

