#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <udis86.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DM_OK		0
#define DM_FAIL		-1

#define DM_RULE \
"----------------------------------------------------------------------------"

struct dm_pht_type {
	int		 type_int;
	char		*type_str;
	char		*descr;
} pht_types[] = {
	{PT_NULL,	"PT_NULL",	"Unused"},
	{PT_LOAD,	"PT_LOAD",	"Loadable segment"},
	{PT_DYNAMIC,	"PT_DYNAMIC",	"Dynamic linking info"},
	{PT_INTERP,	"PT_INTERP",	"Interpreter field"},
	{PT_NOTE,	"PT_NOTE",	"Auxillary info"},
	{PT_SHLIB,	"PT_SHLIB",	"Non-standard"},
	{PT_PHDR,	"PT_PHDR",	"PHT size"},
	{PT_TLS,	"PT_TLS",	"Thread local storage"},
	{PT_LOOS,	"PT_LOOS",	"System specific (lo mark)"},
	{PT_HIOS,	"PT_HIOS",	"System specific (hi mark)"},
	{PT_LOPROC,	"PT_LOPROC",	"CPU specific (lo mark)"},
	{PT_HIPROC,	"PT_HIPROC",	"CPU system-specific (hi mark)"},
};

Elf			*elf = NULL;
FILE			*f;
ud_t			 ud;
long long		 cur_addr;

#define DM_MAX_PROMPT			32
char			prompt[DM_MAX_PROMPT];

/*
 * disassemble a single operation
 */
int
dm_disasm_op(long long addr)
{
	unsigned int		 read;
	char			*hex;

	read = ud_disassemble(&ud);
	hex = ud_insn_hex(&ud);
	printf("    0x%08llx:  %-20s%s\n", addr, hex, ud_insn_asm(&ud));

	return (addr + read);
}

/*
 * get the offset of a section name
 */
long long
dm_find_section(char *find_sec)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	long long		 ret = -1;

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
	}

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
 * make a RWX string for program header flags
 * ret is a preallocated string of atleast 4 in length
 */
#define DM_APPEND_FLAG(x)	*p = (x); p++;
int
dm_make_pht_flag_str(int flags, char *ret)
{
	char			*p = ret;

	memset(ret, 0, 4);

	if (flags & PF_R)
		DM_APPEND_FLAG('R');

	if (flags & PF_W)
		DM_APPEND_FLAG('W');

	if (flags & PF_X)
		DM_APPEND_FLAG('X');

	return (DM_OK);
}

/*
 * show the program header table
 */
int
dm_cmd_pht(char **args)
{
	int			 ret = DM_FAIL;
	GElf_Phdr		 phdr;
	size_t			 num_phdrs, i;
	char			 flags[4];

	if (elf == NULL)
		goto clean;

	if (elf_getphdrnum(elf, &num_phdrs) != 0) {
		fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg ( -1));
		goto clean;
	}

	/* Get program header table */
	printf("\nFound %lu program header records:\n", num_phdrs);
	printf("%s\n", DM_RULE);
	printf("%-10s | %-10s | %-5s | %-10s | %-20s\n",
	    "Offset", "Virtual", "Flags", "Type", "Description");
	printf("%s\n", DM_RULE);

	for (i = 0; i < num_phdrs; i++) {

		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "elf_getphdr: %s", elf_errmsg(-1));
			goto clean;
		}

		dm_make_pht_flag_str(phdr.p_flags, flags);

		printf("0x%08llx | 0x%08llx | %-5s | %-10s | %-20s",
		    (long long) phdr.p_offset, phdr.p_vaddr, flags,
		    pht_types[phdr.p_type].type_str,
		    pht_types[phdr.p_type].descr);

		printf("\n");
	}
	ret = DM_OK;
	printf("%s\n", DM_RULE);
clean:
	printf("\n");
	return (ret);
}

/*
 * dump the section headers
 */
int
dm_cmd_sht(char **args)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	int			 ret = DM_FAIL;

	if (elf == NULL)
		goto clean;

	/* Get section header table */
	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	printf("\nFound %lu section header records:\n", shdrs_idx);
	printf("%s\n", DM_RULE);
	printf("%-20s | %-10s | %-10s\n", "Name", "Offset", "Virtual");
	printf("%s\n", DM_RULE);

	sec = NULL ;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		if (gelf_getshdr(sec, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr: %s", elf_errmsg(-1));
			goto clean;
		}

		if (!(sec_name = elf_strptr(elf, shdrs_idx, shdr.sh_name))) {
			fprintf(stderr, "elf_strptr: %s", elf_errmsg(-1));
			goto clean;
		}

		printf("%-20s | 0x%08llx | 0x%08llx\n", sec_name,
		    (long long) shdr.sh_offset, (long long) shdr.sh_addr);
	}
	printf("%s\n", DM_RULE);

	ret = DM_OK;
clean:
	printf("\n");
	return (ret);
}

int
dm_cmd_info(char **args)
{
	printf("INFO\n");

	return (0);
}

int
dm_seek(long long addr)
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
	long long			to;

	/* seeking to a section? */
	if (args[0][0] == '.')
		to = dm_find_section(args[0]);
	else
		to = strtoll(args[0], NULL, 0);

	dm_seek(to);

	return (0);
}

int
dm_cmd_dis(char **args)
{
	int			ops = strtoll(args[0], NULL, 0), i;
	long long		addr = cur_addr;

	printf("\n");
	for (i = 0; i < ops; i++)
		addr = dm_disasm_op(addr);
	printf("\n");

	dm_seek(cur_addr); /* rewind back */
	return (0);
}

int
dm_cmd_dis_noargs(char **args)
{
	char			*arg ="8";

	dm_cmd_dis(&arg);
	return (0);
}



struct dm_cmd_sw {
	char			*cmd;
	uint8_t			 args;
	int			(*handler)(char **args);
};

struct dm_cmd_sw dm_cmds[] = {
	{"info", 1, dm_cmd_info},	{"i", 1, dm_cmd_info},
	{"seek", 1, dm_cmd_seek},	{"s", 1, dm_cmd_seek},
	{"dis", 1, dm_cmd_dis},		{"pd", 1, dm_cmd_dis},
	{"dis", 0, dm_cmd_dis_noargs},	{"pd", 0, dm_cmd_dis_noargs},
	{"pht", 0, dm_cmd_pht},
	{"sht", 0, dm_cmd_sht},
	{NULL, 0, NULL}
};

#define DM_CMD_MAX_TOKS			8
void
dm_parse_cmd(char *line)
{
	int			 found_toks = 0;
	char			*tok, *next = line;
	char			*toks[DM_CMD_MAX_TOKS];
	struct dm_cmd_sw	*cmd = dm_cmds;

	while ((found_toks < DM_CMD_MAX_TOKS) && (tok = strsep(&next, " "))) {
		toks[found_toks++] = tok;
	}

	while (cmd->cmd != NULL) {
		if ((strcmp(cmd->cmd, toks[0]) != 0) ||
		    (cmd->args != found_toks - 1)) {
			cmd++;
			continue;
		}

		cmd->handler(&toks[1]);
		break;
	}

	if (cmd->cmd == NULL)
		printf("parse error\n");
}

void
dm_update_prompt()
{
	snprintf(prompt, DM_MAX_PROMPT, "[0x%08llx] ", cur_addr);
}

void
dm_interp()
{
	char			*line;

	dm_update_prompt();
	while((line = readline(prompt)) != NULL) {
		if (*line) {
			add_history(line);
			dm_parse_cmd(line);
		}
		dm_update_prompt();
	}
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: XXX\n");
		exit(1);
	}

	if ((f = fopen(argv[1], "r")) == NULL) {
		perror("open");
		exit(1);
	}

	dm_init_elf();

	ud_init(&ud);
	ud_set_input_file(&ud, f);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	/* start at .text */
	dm_seek(dm_find_section(".text"));

	dm_interp();

	return (EXIT_SUCCESS);
}
