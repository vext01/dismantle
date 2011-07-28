#ifndef __CFG_H
#define __CFG_H

#include "common.h"
#include "dm_dis.h"

struct dm_ssa_index {
	enum ud_type 	reg;
	int				index;
};

struct dm_instruction_se {
	enum ud_mnemonic_code	instruction;
	int			write;
	int			jump;
	int			ret;
};

struct dm_cfg_node {
	NADDR start;
	NADDR end;
	struct dm_cfg_node **children;
};

void 			dm_ssa_index_init(struct dm_ssa_index **indices);
void 			dm_instruction_se_init(struct dm_instruction_se **instructions);

int 			dm_cmd_ssa(char **args);
int 			dm_cmd_cfg(char **args);

struct dm_cfg_node* 	dm_new_cfg_node(NADDR nstart, NADDR nend);
//void 			dm_print_cfg(struct dm_cfg_node *node);
struct dm_cfg_node* 	dm_gen_cfg_block(struct dm_cfg_node *node, struct dm_instruction_se *instructions);

struct dm_cfg_node*  	dm_find_cfg_node_starting(struct dm_cfg_node *node, NADDR addr);
struct dm_cfg_node* 	dm_find_cfg_node_containing(struct dm_cfg_node *node, NADDR addr);

#endif
