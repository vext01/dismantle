#include "dm_dis.h"

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
	NADDR			to;

	/* seeking to a section? */
	if (args[0][0] == '.')
		to = dm_find_section(args[0]);
	else
		to = strtoll(args[0], NULL, 0);

	dm_seek(to);

	return (0);
}

/*
 * disassemble a single operation
 */
int
dm_disasm_op(NADDR addr)
{
	unsigned int		 read;
	char			*hex;

	if ((read = ud_disassemble(&ud)) == 0) {
		fprintf(stderr,
			"failed to disassemble at " NADDR_FMT "\n", addr);
		return (0);
	}

	hex = ud_insn_hex(&ud);
	printf("  " NADDR_FMT ":  %-20s%s\n", addr, hex, ud_insn_asm(&ud));

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
