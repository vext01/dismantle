#include <stdio.h>
#include "dm_cfg.h"

/* A linked list to keep track of all the CFG blocks so we can free them at the end */
struct ptrs {
	void* ptr;
	struct ptrs *next;
};
/* Head of the list and a list iterator */
struct ptrs *p_head = NULL;
struct ptrs *p = NULL;

/*
 * Generate static CFG for a function (continues until it reaches ret), does not follow calls
 */
int
dm_cmd_cfg(char **args) {
	NADDR		addr = cur_addr;
	struct		dm_instruction_se *instructions = NULL;	
	struct		dm_cfg_node *cfg = NULL;
	
	(void) args;

	dm_instruction_se_init(&instructions);
	
	/* Initialise free list */
	p_head = calloc(1, sizeof(struct ptrs));
	p = p_head;
	
	/* Create first node */
	cfg = dm_new_cfg_node(addr, 0);
	
	/* Create CFG */	
	dm_gen_cfg_block(cfg, instructions);

	/* Print CFG */
	dm_print_cfg(cfg);
	
	/* Free all memory */
	dm_cfg_free();
	free(instructions);

	/* Rewind back */
	dm_seek(addr);
	return (0);
}

/* nasty hack, overapproximates size of ud enum in itab.h, fix XXX */
#define DM_UD_ENUM_HACK				600
void
dm_instruction_se_init(struct dm_instruction_se **instructions) {
	int i;

	*instructions = malloc(sizeof(struct dm_instruction_se) * (DM_UD_ENUM_HACK));
	
	/* Initialise struct recording which instructions write to registers */
	for (i = 0; i < DM_UD_ENUM_HACK; i++) {
		(*instructions)[i].instruction = i;//(enum ud_mnemonic_code)i;
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

/*
 * Create a new node in the CFG
 */
struct dm_cfg_node* 
dm_new_cfg_node(NADDR nstart, NADDR nend) {
	struct dm_cfg_node* node;
	node = malloc(sizeof(struct dm_cfg_node));
	node->start = nstart;
	node->end = nend;
	node->children = calloc(1, sizeof(void*));
	/* Add this node to the free list so we can free the memory at the end */
	p->ptr = (void*)node;
	p->next = calloc(1, sizeof(struct ptrs));
	p = p->next;
	return node;
}

struct dm_cfg_node* 
dm_gen_cfg_block(struct dm_cfg_node *node, struct dm_instruction_se *instructions) {
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
		if ((foundNode = dm_find_cfg_node_starting(addr)) && (foundNode != node)) {
			addr -= oldRead;
			free(node->children);
			node->children = calloc(2, sizeof(void*));
			node->children[0] = foundNode;
			break;
		}
		
		/* Check for jump instructions and create new nodes as necessary */
		if (instructions[ud.mnemonic].jump) {
			/* End the block here */
			node->end = addr;
			free(node->children);
			/* Make space for the children of this block */	
			node->children = calloc(instructions[ud.mnemonic].jump + 1, sizeof(void*));	
			
			/* Check if we are jumping to the start of an already existing block, if so use that as child of current block */
			if ((node->children[0] = dm_find_cfg_node_starting(ud.operand[0].lval.sdword + addr + read)) == NULL)
				/* Check if we are jumping to the middle of an already existing block, 
				 * if so split it and use 2nd half as child of current block */
				if ((node->children[0] = dm_find_cfg_node_containing(ud.operand[0].lval.sdword + addr + read)) == NULL) {
					/* This is a new block, so scan do a recursive call to find it's start, end, and children */
					node->children[0] = dm_new_cfg_node(ud.operand[0].lval.sdword + addr + read, 0);
					dm_gen_cfg_block(node->children[0], instructions);
				}
			
			/* Seek back to where we were before we followed the jump */
			dm_seek(addr);
			read = ud_disassemble(&ud);
			
			/* If the jump was a conditional, now we must follow the other leg of the jump */
			if (instructions[ud.mnemonic].jump > 1) {
				if ((node->children[1] = dm_find_cfg_node_starting(addr + read)) != NULL)
					break;
				else {
					node->children[1] = dm_new_cfg_node(addr + read, 0);
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
struct dm_cfg_node*
dm_find_cfg_node_starting(NADDR addr) {
	struct dm_cfg_node *node;
	p = p_head;
	while (p->ptr != NULL) {
		node = (struct dm_cfg_node*)(p->ptr);
		if (node->start == addr)
			return node;
		p = p->next;
	}
	return NULL;
}

/* 
 * Searches all blocks to see if one contains addr. If so, split the block and return the tail 
 * (otherwise returns NULL)
 */
struct dm_cfg_node*
dm_find_cfg_node_containing(NADDR addr) {
	struct dm_cfg_node *node, *newNode;
	NADDR addr2;
	unsigned int read = 0;
	p = p_head;
	while (p->ptr != NULL) {
		node = (struct dm_cfg_node*)(p->ptr);
		if ((node->start > addr) && (node->end != 0) && (node->end < addr)) {
			/* We found a matching block
			 * Now find address before addr and splot the block, return tail block */
			for (dm_seek(node->start); addr2 + read != addr; addr2 += read)
				read = ud_disassemble(&ud);
			newNode = dm_new_cfg_node(addr, node->end);
			newNode->children = node->children;
			node->children = calloc(2, sizeof(void*));
			node->children[0] = newNode;
			node->end = addr2;
			return newNode;
		}
		p = p->next;
	}
	return NULL;
}

/*
 * Use the free list to print info on all the blocks we have found
 */
void dm_print_cfg() {
	struct dm_cfg_node* node;
	int i;
	p = p_head;
	while (p->ptr != NULL) {
		node = (struct dm_cfg_node*)(p->ptr);
		printf("Block start: " NADDR_FMT ", end: " NADDR_FMT "\n", 
		    node->start, node->end);
		for (i = 0; node->children[i] != NULL; i++)
			printf("\tChild %d start: " NADDR_FMT ", end: " NADDR_FMT "\n", i, 
			    node->children[i]->start, 
			    node->children[i]->end);
		p = p->next;
	}
}

/* 
 * Free all data structures used for building the CFG
 */
void 
dm_cfg_free() {
	struct ptrs* p_prev;
	p = p_head;
	while (p != NULL) {
		if (p->ptr != NULL) free(((struct dm_cfg_node*)(p->ptr))->children);
		free(p->ptr);
		p_prev = p;
		p = p->next;
		free(p_prev);
	}
}

int 
dm_cmd_ssa(char **args) {
	struct 		dm_ssa_index  *indices = NULL;
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

void 
dm_ssa_index_init(struct dm_ssa_index **indices) {
	int i;

	*indices = malloc(sizeof(struct dm_ssa_index) * (UD_OP_CONST + 1));
	/* Initialise struct for SSA indexes */
	for (i = 0; i < UD_OP_CONST + 1; i++) {
		(*indices)[i].reg = (enum ud_type)i;
		(*indices)[i].index = 0;
	}
}
