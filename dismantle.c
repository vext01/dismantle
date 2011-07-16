#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <udis86.h>

/*
 * disassemble a single operation
 */
int
dm_disasm_op(ud_t *ud, FILE *f, int addr)
{
	unsigned int		 read;
	char			*hex;

	if (fseek(f, addr, SEEK_SET) < 0) {
		perror("seek");
		return (0);
	}

	ud_set_pc(ud, addr);
	read = ud_disassemble(ud);
	hex = ud_insn_hex(ud);

	printf("0x%08x:  %-20s%s\n", addr, hex, ud_insn_asm(ud));
	addr += read;

	return (addr + read);
}

int
main(void)
{
	ud_t			 ud;
	int			 i, addr = 0x00000160;
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
		addr = dm_disasm_op(&ud, f, addr);
	}

	return (EXIT_SUCCESS);
}
