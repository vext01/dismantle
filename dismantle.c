#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <udis86.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>

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

void
dm_parse_elf(FILE *f)
{
	Elf			*elf;
	Elf_Kind		ek;
	GElf_Phdr		phdr;
	size_t			num_phdrs, i;

	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		return;
	}

	if ((elf = elf_begin(fileno(f), ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		return;
	}

	ek = elf_kind(elf);

	printf("Detected binary type: ");
	switch (ek) {
	case ELF_K_AR :
		printf("archive\n");
		break ;
	case ELF_K_ELF :
		printf("ELF\n");
		break ;
	case ELF_K_NONE :
		printf("none\n");
		break ;
	default :
		printf("unknown\n");
	}

	if (elf_getphdrnum(elf, &num_phdrs) != 0) {
		fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg ( -1));
		return;
	}

	printf("Found %u section headers\n", num_phdrs);

	for (i = 0; i < num_phdrs; i ++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg(-1));
			return;
		}

		printf("0x%08x: ", phdr.p_offset);
		printf("%d ", phdr.p_type);
		if (phdr.p_flags & PF_X)
			printf("Executable");
		printf("\n");
	}

	printf("\n");
	(void) elf_end(elf);
}

int
main(int argc, char **argv)
{
	ud_t			 ud;
	int			 i, addr = 0x00000160, ops = 8;
	FILE			*f;

	if (argc == 3) {
		addr = atoi(argv[1]);
		ops = atoi(argv[2]);
	}

	if ((f = fopen("/bin/ls", "r")) == NULL) {
		perror("open");
		exit(1);
	}

	dm_parse_elf(f);

	ud_init(&ud);
	ud_set_input_file(&ud, f);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	for (i = 0; i < ops; i++)
		addr = dm_disasm_op(&ud, f, addr);

	return (EXIT_SUCCESS);
}
