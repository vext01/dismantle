#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <udis86.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <readline/readline.h>
#include <readline/history.h>

Elf			*elf = NULL;
FILE			*f;
ud_t			 ud;
long			 cur_addr;

/*
 * disassemble a single operation
 */
int
dm_disasm_op(long addr)
{
	unsigned int		 read;
	char			*hex;

	if (fseek(f, addr, SEEK_SET) < 0) {
		perror("seek");
		return (0);
	}

	ud_set_pc(&ud, addr);
	read = ud_disassemble(&ud);
	hex = ud_insn_hex(&ud);

	printf("0x%08lx:  %-20s%s\n", addr, hex, ud_insn_asm(&ud));
	addr += read;

	return (addr + read);
}

/*
 * get the offset of a section name
 */
long
dm_find_section(char *find_sec)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	long			 ret = -1;

	if (elf == NULL)
		goto clean;

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

		printf("0x%08lx: %s\n", (long) shdr.sh_offset, sec_name);
	}

	printf("\n");

clean:
	return (ret);
}

int
dm_init_elf()
{
	Elf_Kind		 ek;

	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		goto err;
	}

	if ((elf = elf_begin(fileno(f), ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		goto err;
	}

	ek = elf_kind(elf);

	if (ek != ELF_K_ELF) {
		fprintf(stderr, "Does not appear to have an ELF header\n");
		goto err;
	}

	return (0);
err:
	elf_end(elf);
	elf = NULL;

	return (-1);
}

/*
 * XXX separate
 */
void
dm_dump_elf_info(FILE *f)
{
	GElf_Phdr		 phdr;
	Elf_Scn			*sec;
	size_t			 num_phdrs, shdrs_idx, i;
	GElf_Shdr		 shdr;
	char			*sec_name;

	if (elf == NULL)
		goto clean;

	if (elf_getphdrnum(elf, &num_phdrs) != 0) {
		fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg ( -1));
		goto clean;
	}

	/* Get program header table */
	printf("Found %lu program header records:\n", num_phdrs);

	for (i = 0; i < num_phdrs; i++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "elf_getphdr: %s", elf_errmsg(-1));
			goto clean;
		}

		printf("0x%08lx: %d", (long) phdr.p_offset, phdr.p_type);
		if (phdr.p_flags & PF_X)
			printf(" Executable");
		printf("\n");
	}

	/* Get section header table */
	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	printf("\nFound %lu section header records:\n", shdrs_idx);
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

		printf("0x%08lx: %s\n", (long) shdr.sh_offset, sec_name);
	}

clean:
	printf("\n");
}

#define DM_MAX_PROMPT			16
void
dm_interp()
{
	char			*line;
	char			prompt[DM_MAX_PROMPT];

	snprintf(prompt, DM_MAX_PROMPT, "[0x%08lx] ", cur_addr);

	while((line = readline(prompt)) != NULL) {
		printf("got '%s'\n", line);
		add_history(line);
	}
}

int
main(int argc, char **argv)
{
	int			 i, ops = 8;

	if (argc != 2) {
		printf("Usage: XXX\n");
		exit(1);
	}

	if ((f = fopen(argv[1], "r")) == NULL) {
		perror("open");
		exit(1);
	}

	dm_init_elf();
	dm_dump_elf_info(f);

	ud_init(&ud);
	ud_set_input_file(&ud, f);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	cur_addr = dm_find_section(".text");
	printf("Seeking to .text at %08lx\n", cur_addr);

	for (i = 0; i < ops; i++)
		cur_addr = dm_disasm_op(cur_addr);

	dm_interp();

	return (EXIT_SUCCESS);
}
