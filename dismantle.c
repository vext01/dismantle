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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <udis86.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DM_OK		0
#define DM_FAIL		-1


/* native address size */
#if defined(_M_X64) || defined(__amd64__)
#define NADDR		Elf64_Addr
#define ADDR_FMT_64	"0x%08lx"
#define ADDR_FMT_32	"0x%08x"
#define NADDR_FMT	ADDR_FMT_64
#else
#define NADDR		Elf32_Addr
#define ADDR_FMT_64	"0x%08llx"
#define ADDR_FMT_32	"0x%08x"
#define NADDR_FMT	ADDR_FMT_32
#endif

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
	/* XXX define thses for !glibc systems */
#ifdef __Linux__
        {PT_GNU_EH_FRAME,       "PT_GNU_EH_FRAME",      "GNU extension"},
        {PT_GNU_STACK,          "PT_GNU_STACK",         "GNU extension"},
        {PT_GNU_RELRO,          "PT_GNU_RELRO",         "GNU extension"},
        {PT_LOSUNW,             "PT_LOSUNW",            "Sun extension"},
        {PT_SUNWBSS,            "PT_SUNWBSS",           "Sun extension"},
        {PT_SUNWSTACK,          "PT_SUNWSTACK",         "Sun extension"},
        {PT_HISUNW,             "PT_HISUNW",            "Sun extension"},
#endif
	{-1,		NULL,		NULL},
};
struct dm_pht_type	unknown_pht_type = {-1, "???", "Unknown PHT type"};

struct dm_help_rec {
	char		*cmd;
	char		*descr;
} help_recs[] = {
	{"seek/s addr",		"Seek to an address"},
	{"dis/pd [ops]",	"Disassemble (8 or 'ops' operations)"},
	{"hex/px [len]",	"Dump hex (64 or 'len' bytes)"},
	{"pht",			"Show program header table"},
	{"sht",			"Show section header table"},
	{"/ str",		"Find ASCII string from current pos"},
	{"help/?",		"Show this help"},
	{"CTRL+D",		"Exit"},
	{NULL, 0},
};

Elf			*elf = NULL;
FILE			*f;
struct stat		 bin_stat;
ud_t			 ud;
NADDR		 cur_addr;

#define DM_MAX_PROMPT			32
char			prompt[DM_MAX_PROMPT];

#define DM_HEX_CHUNK		16

/*
 * pretty print bytes from a buffer (as hex)
 */
int
dm_dump_hex_pretty(uint8_t *buf, size_t sz, NADDR start_addr)
{
	size_t			done = 0;

	if (sz > 16)
		return (DM_FAIL);

	printf("  " NADDR_FMT ":  ", start_addr);

	/* first hex view */
	for (done = 0; done < 16; done++) {
		if (done < sz)
			printf("%02x ", buf[done]);
		else
			printf("   ");
	}
	printf("    ");

	/* now ASCII view */
	for (done = 0; done < sz; done++) {
		if ((buf[done] > 0x20) && (buf[done] < 0x7e))
			printf("%c", buf[done]);
		else
			printf(".");
	}
	printf("\n");

	return (DM_OK);
}

/*
 * reads a number of bytes from a file and pretty prints them
 * in lines of 16 bytes
 */
int
dm_dump_hex(size_t bytes)
{
	size_t			orig_pos = ftell(f);
	size_t			done = 0, read = 0, to_read = DM_HEX_CHUNK;
	uint8_t			buf[DM_HEX_CHUNK];

	printf("\n");
	for (done = 0; done < bytes; done += read) {

		if (DM_HEX_CHUNK > bytes - done)
			to_read = bytes - done;

		read = fread(buf, 1, to_read, f);

		if ((!read) && (ferror(f))) {
			perror("failed to read bytes");
			clearerr(f);
			return (DM_FAIL);
		}
		dm_dump_hex_pretty(buf, read, orig_pos + done);

		if ((!read) && (feof(f)))
			break;
	}
	printf("\n");

	if (fseek(f, orig_pos, SEEK_SET) < 0) {
		perror("could not seek file");
		return (DM_FAIL);
	}

	return (DM_OK);
}

struct dm_pht_type *
dm_get_pht_info(int find)
{
	struct dm_pht_type		*t = pht_types;

	while (t->type_int != -1) {
		if (t->type_int == find)
			break;
		t++;
	}

	if (t->type_int == -1)
		return (NULL);

	return (t);
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

/*
 * get the offset of a section name
 */
NADDR
dm_find_section(char *find_sec)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	NADDR		 ret = -1;

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
	struct dm_pht_type	*pht_t;

	(void) args;

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
		pht_t = dm_get_pht_info(phdr.p_type);

		if (!pht_t)
			pht_t = &unknown_pht_type;

		/* offset is ElfAddr_64 bit on every arch for some reason */
		printf(ADDR_FMT_64 " | " ADDR_FMT_64 " | %-5s | %-10s | %-20s",
		    phdr.p_offset, phdr.p_vaddr, flags,
		    pht_t->type_str, pht_t->descr);

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

	(void) args;

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

		printf("%-20s | " ADDR_FMT_64 " | " NADDR_FMT "\n",
		    sec_name, shdr.sh_offset, (NADDR) shdr.sh_addr);
	}
	printf("%s\n", DM_RULE);

	ret = DM_OK;
clean:
	printf("\n");
	return (ret);
}

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

int
dm_cmd_dis(char **args)
{
	int		ops = strtoll(args[0], NULL, 0), i;
	NADDR		addr = cur_addr;

	printf("\n");
	for (i = 0; i < ops; i++) {
		addr = dm_disasm_op(addr);
		if (addr == 0)
			break;
	}
	printf("\n");

	dm_seek(cur_addr); /* rewind back */
	return (0);
}

int
dm_cmd_dis_noargs(char **args)
{
	char			*arg ="8";

	(void) args;

	dm_cmd_dis(&arg);
	return (0);
}

int
dm_cmd_help()
{
	struct dm_help_rec	*h = help_recs;

	printf("\n");
	while (h->cmd != 0) {
		printf("%-15s   %s\n", h->cmd, h->descr);
		h++;
	}
	printf("\n");

	return (DM_OK);
}

int
dm_cmd_hex(char **args)
{
	dm_dump_hex(strtoll(args[0], NULL, 0));
	return (DM_OK);
}

int
dm_cmd_hex_noargs(char **args)
{
	(void) args;
	dm_dump_hex(64);
	return (DM_OK);
}

int
dm_cmd_findstr(char **args)
{
	NADDR			 byte = 0;
	char			*find = args[0], *cmp = NULL;
	size_t			 find_len = strlen(find);
	size_t			 orig_pos = ftell(f), read;
	int			 ret = DM_FAIL;
	int			 hit = 0;

	if (bin_stat.st_size < (off_t) find_len) {
		fprintf(stderr, "file not big enough for that string\n");
		goto clean;
	}

	rewind(f);

	cmp = malloc(find_len);
	for (byte = 0; byte < bin_stat.st_size - find_len; byte++) {

#if 0
		printf("\r" NADDR_FMT "/" NADDR_FMT " (% 3d%%)", byte,
		    (NADDR) (bin_stat.st_size - find_len),
		    (byte / (float) (bin_stat.st_size - find_len) * 100));
#endif

		if (fseek(f, byte, SEEK_SET))
			perror("fseek");

		read = fread(cmp, 1, find_len, f);
		if (!read) {
			if (feof(f))
				break;
			perror("could not read file");
			goto clean;
		}

		if (memcmp(cmp, find, find_len) == 0)
			printf("  HIT %03d: " NADDR_FMT "\n", hit++, byte);
	}

	ret = DM_OK;
clean:
	if (cmp)
		free(cmp);

	if (fseek(f, orig_pos, SEEK_SET))
		perror("fseek");

	printf("\n");
	return (DM_OK);
}

struct dm_cmd_sw {
	char			*cmd;
	uint8_t			 args;
	int			(*handler)(char **args);
} dm_cmds[] = {
	{"seek", 1, dm_cmd_seek},	{"s", 1, dm_cmd_seek},
	{"dis", 1, dm_cmd_dis},		{"pd", 1, dm_cmd_dis},
	{"dis", 0, dm_cmd_dis_noargs},	{"pd", 0, dm_cmd_dis_noargs},
	{"pht", 0, dm_cmd_pht},
	{"sht", 0, dm_cmd_sht},
	{"hex", 1, dm_cmd_hex},		{"px", 1, dm_cmd_hex},
	{"findstr", 1, dm_cmd_findstr},	{"/", 1, dm_cmd_findstr},
	{"hex", 0, dm_cmd_hex_noargs},	{"px", 0, dm_cmd_hex_noargs},
	{"help", 0, dm_cmd_help},	{"?", 0, dm_cmd_help},
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
	snprintf(prompt, DM_MAX_PROMPT, NADDR_FMT " dm> ", cur_addr);
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
		printf("Usage: dismantle <elf binary>\n");
		exit(1);
	}

	if ((f = fopen(argv[1], "r")) == NULL) {
		perror("open");
		exit(1);
	}

	if (fstat(fileno(f), &bin_stat) < 0) {
		perror("fstat");
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
