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

ud_t ud;
NADDR cur_addr;

int
dm_seek(NADDR addr)
{
	cur_addr = addr;

	if (fseek(f, cur_addr, SEEK_SET) < 0) {
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
	struct dm_dwarf_sym_cache_entry	*sym;

	/* seeking to a section? */
	if (args[0][0] == '.')
		to = dm_find_section(args[0]);
	else {
		/* we first try to find a dwarf sym of that name */
		if (dm_dwarf_find_sym(args[0], &sym) == DM_OK)
			to = sym->offset;
		else
			to = strtoll(args[0], NULL, 0);
	}

	dm_seek(to);
	return (DM_OK);
}

/*
 * disassemble a single operation
 */
int
dm_disasm_op(NADDR addr)
{
	unsigned int		 read;
	char			*hex;
	NADDR			 target = 0;

	if ((read = ud_disassemble(&ud)) == 0) {
		fprintf(stderr,
			"failed to disassemble at " NADDR_FMT "\n", addr);
		return (0);
	}

	hex = ud_insn_hex(&ud);

	printf("  " NADDR_FMT ":  %-20s%s", addr, hex, ud_insn_asm(&ud));

	if (ud.mnemonic == UD_Icall) {

		switch (ud.operand[0].size) {
		case 8:
			if (ud.br_far)
				target = ud.operand[0].lval.ubyte;
			else
				target = ud.pc + ud.operand[0].lval.sbyte;
			break;
		case 16:
			if (ud.br_far)
				target = ud.operand[0].lval.uword;
			else
				target = ud.pc + ud.operand[0].lval.sword;
			break;
		case 32:
			if (ud.br_far)
				target = ud.operand[0].lval.udword;
			else
				target = ud.pc + ud.operand[0].lval.sdword;
			break;
		case 64:
			if (ud.br_far)
				target = ud.operand[0].lval.uqword;
			else
				target = ud.pc + ud.operand[0].lval.sqword;
			break;
		default:
			fprintf(stderr, "unknown operand size");
		}

		/* if the target was a symbol we know, then say so */
		struct dm_dwarf_sym_cache_entry			*sym;
		if (dm_dwarf_find_sym_at_offset(target, &sym) == DM_OK) {
			printf("\t(%s)", sym->name);

		}
	}

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
dm_cmd_dis_noargs(char **args)
{
	(void) args;

	char	*arg ="8";

	dm_cmd_dis(&arg);
	return (0);
}
