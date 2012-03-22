/*
 * Copyright (c) 2011, Edd Barrett <vext01@gmail.com>
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


#include "dm_dis.h"
#include "dm_dwarf.h"

ud_t			ud;
NADDR			cur_addr;

int
dm_seek(NADDR addr)
{
	cur_addr = addr;

	if (fseek(file_info.fptr, cur_addr, SEEK_SET) < 0) {
		perror("seek");
		return (-1);
	}

	ud_set_pc(&ud, cur_addr);

	return (0);
}

int
dm_cmd_seek(char **args)
{
	NADDR				 to;
	int				 ret = DM_FAIL;
	struct dm_dwarf_sym_cache_entry	*sym;
	GElf_Shdr			 shdr;

	/* seeking to a section? */
	if (args[0][0] == '.') {
		if ((dm_find_section(args[0], &shdr)) == DM_FAIL) {
			DPRINTF(DM_D_WARN,
			    "section non-existant: %s", args[0]);
			goto clean;
		}
		to = shdr.sh_offset;
	} else {
		/* we first try to find a dwarf sym of that name */
		if (dm_dwarf_find_sym(args[0], &sym) == DM_OK)
			to = sym->offset;
		else
			to = strtoll(args[0], NULL, 0);
	}

	dm_seek(to);
	ret = DM_OK;
clean:
	return (ret);
}

/*
 * disassemble a single operation
 */
int
dm_disasm_op(NADDR addr)
{
	struct dm_dwarf_sym_cache_entry		*sym, *label_sym;
	unsigned int				 read;
	char					*hex;
	NADDR					 target = 0;
	uint8_t					 colour_set = 0;

	if ((read = ud_disassemble(&ud)) == 0) {
		fprintf(stderr,
			"failed to disassemble at " NADDR_FMT "\n", addr);
		return (DM_FAIL);
	}

	/* if we know this symbol name, print it as a label */
	if (dm_dwarf_find_sym_at_offset(addr, &label_sym) == DM_OK)
		printf("%s\n  %s%s():%s\n%s\n", DM_RULE, ANSII_GREEN,
		    label_sym->name, ANSII_WHITE, DM_RULE);

	hex = ud_insn_hex(&ud);

	/* colourise control flow */
	if ((ud.br_far) || (ud.br_near)) {
		/* jumps and calls are yellow */
		printf(ANSII_YELLOW);
		colour_set = 1;
	} else if ((ud.mnemonic == UD_Iret) || (ud.mnemonic == UD_Iretf)) {
		/* returns are red */
		printf(ANSII_RED);
		colour_set = 1;
	}

	printf("    " NADDR_FMT ":  %-20s%s", addr, hex, ud_insn_asm(&ud));

	if (ud.mnemonic == UD_Icall) {
		target = dm_get_jump_target(ud);
		/* if the target was a symbol we know, then say so */
		if (dm_dwarf_find_sym_at_offset(target, &sym) == DM_OK) {
			printf("\t(%s)", sym->name);

		}
	}

	if (colour_set) /* reset colour */
		printf(ANSII_WHITE);

	printf("\n");

	return (addr + read);
}

int
dm_cmd_dis(char **args)
{
	int	ops = strtoll(args[0], NULL, 0), i;
	NADDR	addr = cur_addr;

	printf("\n");
	for (i = 0; i < ops; i++) {
		addr = dm_disasm_op(addr);
		if (!addr)
			break;
	}
	printf("\n");

	dm_seek(cur_addr); /* rewind back */
	return (0);
}

int
dm_cmd_dis_func(char **args)
{
	NADDR	addr = cur_addr;

	(void) args;

	printf("\n");
	do {
		addr = dm_disasm_op(addr);
		if (!addr)
			break;
	} while (ud.mnemonic != UD_Iret);
	printf("\n");

	dm_seek(cur_addr); /* rewind back */
	return (DM_OK);
}

int
dm_cmd_dis_noargs(char **args)
{
	(void) args;

	char	*arg ="8";

	dm_cmd_dis(&arg);
	return (0);
}

NADDR
dm_get_jump_target(struct ud ud)
{
	return dm_get_operand_lval(ud, 0, 0, 1);
}


NADDR
dm_get_operand_lval(struct ud ud, int op, int offset, int chkfar)
{
	NADDR	target;
	int	switch_target = 0;
	if (offset)
		switch_target = ud.operand[op].offset;
	else
		switch_target = ud.operand[op].size;
	switch (switch_target) {
		case 8:
			if (ud.br_far || !chkfar)
				target = ud.operand[op].lval.ubyte;
			else
				target = ud.pc + ud.operand[op].lval.sbyte;
			break;
		case 16:
			if (ud.br_far || !chkfar)
				target = ud.operand[op].lval.uword;
			else
				target = ud.pc + ud.operand[op].lval.sword;
			break;
		case 32:
			if (ud.br_far || !chkfar)
				target = ud.operand[op].lval.udword;
			else
				target = ud.pc + ud.operand[op].lval.sdword;
			break;
		case 64:
			if (ud.br_far || !chkfar)
				target = ud.operand[op].lval.uqword;
			else
				target = ud.pc + ud.operand[op].lval.sqword;
			break;
		default:
			fprintf(stderr, "unknown operand size");
	}
	return target;
}

/*
 * set bits to 32 or 64
 */
int
dm_cmd_bits(char **args)
{
	uint8_t			b = atoi(args[0]);

	if ((b != 32) && (b != 64)) {
		printf("Say whaaaaat? 32 or 64\n");
		return (DM_FAIL);
	}

	file_info.bits = b;
	ud_set_mode(&ud, b);

	return (DM_OK);
}

int
dm_cmd_bits_noargs(char **args)
{
	(void) args;

	printf("  %d\n", file_info.bits);
	return (DM_OK);
}


