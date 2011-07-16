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

/*
 * get the offset of a section name
 */
long
dm_find_section(FILE *f, char *find_sec)
{
	Elf			*elf;
	Elf_Kind		 ek;
	Elf_Scn			*sec;
	size_t			 shdrs_idx, i;
	GElf_Shdr		 shdr;
	char			*sec_name;
	long			 ret = -1;

	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		goto clean;
	}

	if ((elf = elf_begin(fileno(f), ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		goto clean;
	}

	ek = elf_kind(elf);

	if (ek != ELF_K_ELF) {
		fprintf(stderr, "Does not appear to have an ELF header\n");
		goto clean;
	}

	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	sec = NULL ;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		if (gelf_getshdr(sec, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr: %s", elf_errmsg(-1));
			goto clean;
		}

		if ((sec_name =
		    elf_strptr(elf, shdrs_idx, shdr.sh_name)) == NULL) {
			fprintf(stderr, "elf_strptr: %s", elf_errmsg(-1));
			goto clean;
		}

		if (strcmp(sec_name, find_sec) == 0) {
			ret = shdr.sh_offset;
			break;
		}

		printf("0x%08x: %s\n", (long) shdr.sh_offset, sec_name);
	}

	printf("\n");

clean:
	elf_end(elf);
	return (ret);
}



void
dm_dump_elf_info(FILE *f)
{
	Elf			*elf;
	Elf_Kind		 ek;
	GElf_Phdr		 phdr;
	Elf_Scn			*sec;
	size_t			 num_phdrs, shdrs_idx, i;
	GElf_Shdr		 shdr;
	char			*sec_name;

	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		goto clean;
	}

	if ((elf = elf_begin(fileno(f), ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		goto clean;
	}

	ek = elf_kind(elf);

	if (ek != ELF_K_ELF) {
		fprintf(stderr, "Does not appear to have an ELF header\n");
		goto clean;
	}

	if (elf_getphdrnum(elf, &num_phdrs) != 0) {
		fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg ( -1));
		goto clean;
	}

	/* Get program header table */
	printf("Found %u program header records:\n", num_phdrs);

	for (i = 0; i < num_phdrs; i++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "elf_getphdr: %s", elf_errmsg(-1));
			goto clean;
		}

		printf("0x%08x: %d", (long) phdr.p_offset, phdr.p_type);
		if (phdr.p_flags & PF_X)
			printf("Executable");
		printf("\n");
	}

	/* Get section header table */
	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	printf("\nFound %d section header records:\n", shdrs_idx);
	sec = NULL ;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		if (gelf_getshdr(sec, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr: %s", elf_errmsg(-1));
			goto clean;
		}

		if ((sec_name = elf_strptr(elf, shdrs_idx, shdr.sh_name))
		    == NULL) {
			fprintf(stderr, "elf_strptr: %s", elf_errmsg(-1));
			goto clean;
		}

		printf("0x%08x: %s\n", (long) shdr.sh_offset, sec_name);
	}

	printf("\n");

clean:
	elf_end(elf);
}

int
main(int argc, char **argv)
{
	ud_t			 ud;
	int			 i, ops = 8;
	FILE			*f;
	long			addr;

	if (argc == 3) {
		addr = atoi(argv[1]);
		ops = atoi(argv[2]);
	}

	if ((f = fopen("/bin/ls", "r")) == NULL) {
		perror("open");
		exit(1);
	}

	dm_dump_elf_info(f);

	ud_init(&ud);
	ud_set_input_file(&ud, f);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	addr = dm_find_section(f, ".text");
	printf("Seeking to .text at %08x\n", addr);

	for (i = 0; i < ops; i++)
		addr = dm_disasm_op(&ud, f, addr);

	return (EXIT_SUCCESS);
}
