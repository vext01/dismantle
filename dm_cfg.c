#include "dm_cfg.h"

void dm_ssa_index_init(struct dm_ssa_index **indices) {
	int i;
	free (*indices);
	*indices = malloc(sizeof(struct dm_ssa_index) * (UD_OP_CONST + 1));
	// Initialise struct for SSA indexes
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		(*indices)[i].reg = (enum ud_type)i;
		(*indices)[i].index = 0;
	}
}

/*
 * XXX HACK HACK HACK
 * There is no way to get the maximum size of an enum and udis86 keeps
 * reordering them. We really need a linked list here, but for now we
 * over-approximate the size of the enum. Yuck.
 *
 * Add a statically allocated array and and provide a search func.
 */
#define UD_OP_ENUM_SZ			600
void dm_instruction_se_init(struct dm_instruction_se **instructions) {
	int			i;

	free (*instructions);
	*instructions = malloc(sizeof(struct dm_instruction_se) * UD_OP_ENUM_SZ);

	// Initialise struct recording which instructions write to registers
	for (i = 0; i < UD_OP_ENUM_SZ; i++) {
		(*instructions)[i].instruction = (enum ud_mnemonic_code)i;
		(*instructions)[i].write = 1;
		(*instructions)[i].jump = 0;
		(*instructions)[i].ret = 0;
	}

	(*instructions)[UD_Ipush].write = 0;
	(*instructions)[UD_Itest].write = 0;

	(*instructions)[UD_Iret].write = 0;
	(*instructions)[UD_Iret].ret = 1;

	(*instructions)[UD_Ijmp].write = 0;
	(*instructions)[UD_Ijmp].jump = 1;

	(*instructions)[UD_Ijz].write = 0;
	(*instructions)[UD_Ijz].jump = 2;

	(*instructions)[UD_Ijnz].write = 0;
	(*instructions)[UD_Ijnz].jump = 2;

	(*instructions)[UD_Icmp].write = 0;

	(*instructions)[UD_Ijg].write = 0;
	(*instructions)[UD_Ijg].jump = 2;

	(*instructions)[UD_Ijle].write = 0;
	(*instructions)[UD_Ijle].jump = 2;

	(*instructions)[UD_Icall].write = 0;
	(*instructions)[UD_Icall].jump = 0;
}

int dm_cmd_ssa(char **args) {
	struct		dm_ssa_index  *indices = NULL;
	struct		dm_instruction_se *instructions = NULL;
	unsigned int	read;
	char		*hex;
	int		ops = strtoll(args[0], NULL, 0), i;
	NADDR		addr = cur_addr;

	dm_ssa_index_init(&indices);
	dm_instruction_se_init(&instructions);

	printf("\n");
	for (i = 0; i < ops; i++) {
		read = ud_disassemble(&ud);
		hex = ud_insn_hex(&ud);
		printf("    " NADDR_FMT ":  %-20s%s  ",
		    addr, hex, ud_insn_asm(&ud));
		addr = addr + read;
		if (instructions[ud.mnemonic].write)
			indices[ud.operand[0].base].index++;
		/*sprintf(out, "%s", ud_insn_asm(&ud));
		if ((srch = strchr(out, ',')) != NULL)
		printf(*/

		printf("%d, ", indices[ud.operand[0].base].index);
		printf("%d, ", indices[ud.operand[1].base].index);
		//	if (ud.operand[2] != NULL)
		printf("%d\n", indices[ud.operand[2].base].index);
		//}
	}
	
	dm_seek(cur_addr); /* rewind back */
	printf("\n");
	return (0);
}

/* XXX free cfg tree */

struct dm_cfg_node *cfg = NULL;

struct dm_cfg_node*  dm_find_cfg_node_starting(struct dm_cfg_node *node, NADDR addr) {
	int i = 0;
	struct dm_cfg_node *retNode = NULL;
	if (node->start == addr)
		return node;
	else
		for (; node->children[i] != NULL; i++)
			if ((retNode = dm_find_cfg_node_starting(node->children[i], addr)) != NULL)
				return retNode;
	return NULL; 
}

void dm_print_cfg(struct dm_cfg_node *node) {
	int i = 0;
	printf("Block start: " NADDR_FMT ", end: " NADDR_FMT,
	    (unsigned int) node->start, (unsigned int) node->end);

	for (;node->children[i] != NULL; i++)
		printf("Child %d start: " NADDR_FMT "x, end: " NADDR_FMT, i,
		    (unsigned int)node->children[i]->start,
		    (unsigned int)node->children[i]->end);
	printf("\n");
	for (i = 0; node->children[i] != NULL; i++)
		dm_print_cfg(node->children[i]);
}

struct dm_cfg_node* dm_new_cfg_node(NADDR nstart, NADDR nend,
    struct dm_cfg_node *parent) {
	struct dm_cfg_node* node;
	node = malloc(sizeof(struct dm_cfg_node));
	node->start = nstart;
	node->end = nend;
	node->previous = parent;
	node->children = calloc(1, sizeof(void*));
	return node;
}

struct dm_cfg_node* dm_find_cfg_node_containing(struct dm_cfg_node *node,
    NADDR addr) {
	int i = 0;
	struct dm_cfg_node* newNode;
	NADDR addr2 = 0;
	unsigned int read = 0;
	if ((node->start > addr) && (node->end != 0) && (node->end < addr)) {
		/* now find address before this one */
		for (dm_seek(node->start); addr2 + read != addr; addr2 += read)
			read = ud_disassemble(&ud);
		node->end = addr2;
		newNode = dm_new_cfg_node(node->start, addr, node->previous);
		node->start = addr;
		node->previous = newNode;
		return node;
	}
	else
		for (;node->children[i] != NULL; i++)
			if (dm_find_cfg_node_containing(node->children[i], addr) != NULL)
				return node->children[i];
	return NULL;
}

struct dm_cfg_node* dm_gen_cfg_block(struct dm_cfg_node *node, struct dm_instruction_se *instructions) {
	NADDR 			addr = node->start;
	unsigned int 		read = 0, oldRead = 0;
	char 			*hex;
	struct dm_cfg_node	*foundNode = NULL;

	dm_seek(node->start);
	while (1) {
		oldRead = read;
		read = ud_disassemble(&ud);
		hex = ud_insn_hex(&ud);
		
		/* Check we haven't run into the start of another block */
		if ((foundNode = dm_find_cfg_node_starting(cfg, addr)) && (foundNode != node)) {
			addr -= oldRead;
			free(node->children);
			node->children = calloc(2, sizeof(void*));
			node->children[0] = foundNode;
			break;
		}
		
		/* Check for jump instructions and create new nodes as necessary */
		if (instructions[ud.mnemonic].jump) {
			node->end = addr;
			free(node->children);		
			node->children = calloc(instructions[ud.mnemonic].jump + 1, sizeof(void*));	
			
			if ((node->children[0] = dm_find_cfg_node_starting(cfg, ud.operand[0].lval.sdword + addr + read)) == NULL)
				if ((node->children[0] = dm_find_cfg_node_containing(cfg, ud.operand[0].lval.sdword + addr + read)) == NULL) {
					node->children[0] = dm_new_cfg_node(ud.operand[0].lval.sdword + addr + read, 0, node);
					dm_gen_cfg_block(node->children[0], instructions);
				}
			
			/* Seek back to where we were before jump */
			dm_seek(addr);
			read = ud_disassemble(&ud);
			
			if (instructions[ud.mnemonic].jump > 1) {
				if ((node->children[1] = dm_find_cfg_node_starting(cfg, addr + read)) != NULL)
					break;
				else {
					node->children[1] = dm_new_cfg_node(addr + read, 0, node);
					node = node->children[1];
				}
			}
			else
				break;
		}
		if (instructions[ud.mnemonic].ret)
			break;
		addr += read;
	}
	node->end = addr;
	return node;
}

/*
 * Generate a static CFG for a function (continues until it reaches ret), does not follow calls
 */

int
dm_cmd_cfg(char **args) {
	NADDR           	addr = cur_addr;
	struct          	dm_instruction_se *instructions = NULL;	

	(void) args;
	
	dm_instruction_se_init(&instructions);

	cfg = dm_new_cfg_node(addr, 0, NULL);	
	dm_gen_cfg_block(cfg, instructions);

	dm_print_cfg(cfg);

	dm_seek(cur_addr); /* rewind back */
	return (0);
}
