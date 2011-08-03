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
#include "dm_dom.h"
#include "dm_cfg.h"
#include "dm_gviz.h"

extern struct ptrs *p_head;
extern struct ptrs *p;
extern int p_length;
extern void **rpost;

/*
 *
 */

int
dm_cmd_dom(char **args)
{
	struct dm_cfg_node	*cfg = NULL, *node = NULL;
	int			i = 0, j = 0;

	(void) args;

	/* Initialise structures */
	dm_init_cfg();

	/* Get CFG */
	cfg = dm_recover_cfg();

	/* Build dominator tree */
	dm_dom(cfg);

	/* Build dominance frontier sets*/
	dm_dom_frontiers();

	/* Print dominator info */
	for (i = 0; i < p_length; i++) {
                node = (struct dm_cfg_node*)rpost[i];
                printf("Block %d (start: " NADDR_FMT ", end: " NADDR_FMT
		    ")\n\tImmediate dominator: %d\n", node->post, node->start,
		    node->end, node->idom->post);
		if (node->df_count) {
			printf("\tDominance frontier set: ");
			for (j = 0; j < node->df_count; j++)
				printf("%d ", node->dom_frontiers[j]->post);
			printf("\n");
		}
        }

	/* Display dominator tree */
	dm_graph_dom();

	/* Free all CFG structures */
	dm_free_cfg();

	return (0);
}

/*
 * Find immediate dominators of all nodes in CFG
 */
void
dm_dom(struct dm_cfg_node *cfg)
{
	struct dm_cfg_node	*node = NULL, *new_idom = NULL;
	int			 changed = 1, i = 0, j = 0, k = 0;

	/* We use the 'visited' field to indicate a node has been processed */
	for (i = 0; i < p_length; i++)
		((struct dm_cfg_node*)rpost[i])->visited = 0;

	/* First node dominates itself */
	cfg->idom = cfg;
	cfg->visited = 1;

	while (changed) {
		changed = 0;
		/* For all nodes except start node, in reverse post-order */
		for (i = 1; i < p_length; i++) {
			node = (struct dm_cfg_node*)rpost[i];

			/* new_idom = first processed parent of node */
			for (j = 0; j < node->p_count; j++)
				if (node->parents[j]->visited) {
					new_idom = node->parents[j];
					break;
				}

			/* For all other parents of node */
			for (k = 0; k < node->p_count; k++)
				if ((j != k) && (node->parents[k]->idom !=
				    NULL))
					new_idom =
					    dm_intersect(node->parents[k],
					    new_idom);
			if (node->idom != new_idom) {
				node->idom = new_idom;
				changed = 1;
			}
			node->visited = 1;
		}
	}
}

/*
 * Find intersecting dominator of two nodes
 */
struct dm_cfg_node*
dm_intersect(struct dm_cfg_node *b1, struct dm_cfg_node *b2)
{
	struct dm_cfg_node	*finger1 = b1, *finger2 = b2;
	while (finger1->post != finger2->post) {
		while (finger1->post < finger2->post)
			finger1 = finger1->idom;
		while (finger2->post < finger1->post)
			finger2 = finger2->idom;
	}
	return finger1;
}

/*
 * Build dominance frontier sets for all nodes
 */
void
dm_dom_frontiers()
{
	struct dm_cfg_node *node = NULL, *runner = NULL;
	int i = 0, j = 0, duplicate = 0;

	/* For all nodes */
	for (p = p_head; p->ptr != NULL; p = p->next) {
		node = (struct dm_cfg_node*)p->ptr;
		/* For all parents of node */
		for (i = 0; (i < node->p_count) && (node->p_count > 1); i++) {
			runner = node->parents[i];
			while (runner != node->idom) {
				/* Don't add duplicate nodes to the set */
				duplicate = 0;
				for (j = 0; j < runner->df_count; j++)
					if (runner->dom_frontiers[j] == node) {
						duplicate = 1;
						break;
					}
				/* Add node to runners frontier set */
				if (!duplicate) {
					runner->dom_frontiers = realloc(
					    runner->dom_frontiers,
					    ++runner->df_count * sizeof(void*));
					runner->dom_frontiers[
					    runner->df_count - 1] = node;
				}
				runner = runner->idom;
			}
		}
	}


}

/*
 * Build a graphviz graph of the dominator tree and display it
 */
void
dm_graph_dom()
{
	struct dm_cfg_node *node = NULL;
	FILE *fp = dm_new_graph("dom.dot");
	char *itoa1 = NULL, *itoa2 = NULL;

	if (!fp) return;

	for (p = p_head; p->ptr != NULL; p = p->next) {
		node = (struct dm_cfg_node*)(p->ptr);

		asprintf(&itoa1, "%d", node->post);
		asprintf(&itoa2, "%d\\nstart: " NADDR_FMT "\\nend: "
		    NADDR_FMT, node->post, node->start, node->end);
		dm_add_label(fp, itoa1, itoa2);
		free(itoa2);

		if (node != node->idom) {
			asprintf(&itoa2, "%d", node->idom->post);
			dm_add_edge(fp, itoa2, itoa1);
			free(itoa2);
		}

		free(itoa1);
	}
	dm_end_graph(fp);
	dm_display_graph("dom.dot");
}

/*void
dm_get_predecessors(struct dm_cfg_node *node, struct ptrs *predecessors)
{
	int c = 0;
	for (; c < node->p_count; c++) {
		predecessors->ptr = node->parents[c];
		predecessors->next = calloc(1, sizeof(struct ptrs));
		predecessors = predecessors->next;
		dm_get_predecessors(node->parents[c], predecessors);
	}
}*/

