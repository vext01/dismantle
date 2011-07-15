#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <udis86.h>

/*
 * disassemble a single operation
 */
int
dm_disasm_op(ud_t *ud, int addr)
{
	unsigned int		 read;
	char			*hex;

	ud_set_pc(ud, addr);
	read = ud_disassemble(ud);
	hex = ud_insn_hex(ud);

	printf("0x%08x:  %-10s\t\t\t%s\n", addr, hex, ud_insn_asm(ud));
	addr += read;

	return (addr + read);
}

int
main(void)
{

	ud_t			 ud;
	int			 i, addr = 0x00000230;
	FILE			*f;

	if ((f = fopen("/bin/ls", "r")) == NULL) {
		perror("open");
		exit(1);
	}

	ud_init(&ud);
	ud_set_input_file(&ud, f);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	for (i = 0; i < 8; i++) {
		addr = dm_disasm_op(&ud, addr);
	}

	return (EXIT_SUCCESS);
}
