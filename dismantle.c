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
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "dm_dis.h"
#include "dm_elf.h"
#include "dm_cfg.h"
#include "dm_dom.h"
#include "dm_ssa.h"
#include "dm_dwarf.h"

uint8_t				 colours_on = 1;

char				 *debug_names[] = {
				    "error", "warn", "info", "debug"};

char *banner =
"        	       ___                            __  __   \n"
"         	  ____/ (_)________ ___  ____ _____  / /_/ /__ \n"
"        	 / __  / / ___/ __ `__ \\/ __ `/ __ \\/ __/ / _ \\\n"
"        	/ /_/ / (__  ) / / / / / /_/ / / / / /_/ /  __/\n"
"        	\\__,_/_/____/_/ /_/ /_/\\__,_/_/ /_/\\__/_/\\___/ \n"
"\n"
"        	      Small i386/amd64 binary browser\n"
"\n"
"        	  (c) Edd Barrett 2011	<vext01@gmail.com>\n"
"        	  (c) Ed Robbins 2011	<edd.robbins@gmail.com>\n";

int				 dm_debug = DM_D_WARN;
struct dm_file_info		file_info;

int	dm_cmd_help();
void	dm_parse_cmd(char *line);
void	dm_update_prompt();
void	dm_interp();
int	dm_dump_hex_pretty(uint8_t *buf, size_t sz, NADDR start_addr);
int	dm_dump_hex(size_t bytes);
int	dm_cmd_hex(char **args);
int	dm_cmd_hex_noargs(char **args);
int	dm_cmd_findstr(char **args);
int	dm_cmd_info(char **args);
int	dm_cmd_debug(char **args);
int	dm_cmd_debug_noargs(char **args);
int	dm_cmd_ansii_noargs(char **args);
int	dm_cmd_ansii(char **args);

struct dm_cmd_sw {
	char			*cmd;
	uint8_t			 args;
	int			(*handler)(char **args);
} dm_cmds[] = {
	{"ansii", 0, dm_cmd_ansii_noargs}, {"ansii", 1, dm_cmd_ansii},
	{"bits", 0, dm_cmd_bits_noargs},
	{"bits", 1, dm_cmd_bits},
	{"cfg", 0, dm_cmd_cfg},
	{"debug", 0, dm_cmd_debug_noargs},
	{"debug", 1, dm_cmd_debug},
	{"dis", 0, dm_cmd_dis_noargs},	{"pd", 0, dm_cmd_dis_noargs},
	{"dis", 1, dm_cmd_dis},		{"pd", 1, dm_cmd_dis},
	{"dom", 0, dm_cmd_dom},
	{"disf", 0, dm_cmd_dis_func},	{"pdf", 0, dm_cmd_dis_func},
	{"findstr", 1, dm_cmd_findstr}, {"/", 1, dm_cmd_findstr},
	{"funcs", 0, dm_cmd_dwarf_funcs}, {"f", 0, dm_cmd_dwarf_funcs},
	{"help", 0, dm_cmd_help},	{"?", 0, dm_cmd_help},
	{"hex", 0, dm_cmd_hex_noargs},  {"px", 0, dm_cmd_hex_noargs},
	{"hex", 1, dm_cmd_hex},         {"px", 1, dm_cmd_hex},
	{"info", 0, dm_cmd_info},	{"i", 0, dm_cmd_info},
	{"offset", 1, dm_cmd_offset},
	{"pht", 0, dm_cmd_pht},
	{"seek", 1, dm_cmd_seek},	{"s", 1, dm_cmd_seek},
	{"sht", 0, dm_cmd_sht},
	{"ssa", 0, dm_cmd_ssa},
	{NULL, 0, NULL}
};

struct dm_help_rec {
	char		*cmd;
	char		*descr;
} help_recs[] = {
	{"  / str",		"Find ASCII string from current pos"},
	{"  CTRL+D",		"Exit"},
	{"  ansii",		"Get/set ANSII colours setting"},
	{"  bits [set_to]",	"Get/set architecture (32 or 64)"},
	{"  cfg",		"Show static CFG for current function"},
	{"  debug [level]",	"Get/set debug level (0-3)"},
	{"  dis/pd [ops]",	"Disassemble (8 or 'ops' operations)"},
	{"  disf/pdf",		"Disassemble function (up until the next RET)"},
	{"  dom",		"Show dominance tree and frontiers of cur func"},
	{"  funcs/f",		"Show functions from dwarf data"},
	{"  help/?",		"Show this help"},
	{"  hex/px [len]",	"Dump hex (64 or 'len' bytes)"},
	{"  info/i",		"Show file information"},
	{"  pht",		"Show program header table"},
	{"  seek/s addr",	"Seek to an address"},
	{"  sht",		"Show section header table"},
	{"  ssa",		"Output SSA form of current function"},
	{NULL, 0},
};

#define DM_MAX_PROMPT			32
char			prompt[DM_MAX_PROMPT];

#define DM_HEX_CHUNK           16
/*
 * pretty print bytes from a buffer (as hex)
 */
int
dm_dump_hex_pretty(uint8_t *buf, size_t sz, NADDR start_addr)
{
	size_t	done = 0;

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
	size_t		orig_pos = ftell(file_info.fptr);
	size_t		done = 0, read = 0, to_read = DM_HEX_CHUNK;
	uint8_t		buf[DM_HEX_CHUNK];

	printf("\n");
	for (done = 0; done < bytes; done += read) {
		if (DM_HEX_CHUNK > bytes - done)
			to_read = bytes - done;

		read = fread(buf, 1, to_read, file_info.fptr);

		if ((!read) && (ferror(file_info.fptr))) {
			perror("failed to read bytes");
			clearerr(file_info.fptr);
			return (DM_FAIL);
		}
		dm_dump_hex_pretty(buf, read, orig_pos + done);

		if ((!read) && (feof(file_info.fptr)))
			break;
	}

	if (fseek(file_info.fptr, orig_pos, SEEK_SET) < 0) {
		perror("could not seek file");
		return (DM_FAIL);
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
dm_cmd_info(char **args)
{
	(void) args;

	printf("  %-16s %s\n", "Filename:", file_info.name);
	printf("  %-16s " NADDR_FMT, "Size:", file_info.stat.st_size);
	printf("  %-16s %hd\n", "Bits:", file_info.bits);
	printf("  %-16s %s\n", "Ident:", file_info.ident);
	printf("  %-16s %hd\n", "ELF:", file_info.elf);
	printf("  %-16s %hd\n", "DWARF:", file_info.dwarf);

	return (DM_OK);
}

/* XXX when we have more search funcs, move into dm_search.c */
int
dm_cmd_findstr(char **args)
{
	NADDR                    byte = 0;
	char                    *find = args[0], *cmp = NULL;
	size_t                   find_len = strlen(find);
	size_t                   orig_pos = ftell(file_info.fptr), read;
	int                      ret = DM_FAIL;
	int                      hit = 0;

	if (file_info.stat.st_size < (off_t) find_len) {
		fprintf(stderr, "file not big enough for that string\n");
		goto clean;
	}

	rewind(file_info.fptr);

	cmp = malloc(find_len);
	for (byte = 0; byte < file_info.stat.st_size - find_len; byte++) {

		if (fseek(file_info.fptr, byte, SEEK_SET))
			perror("fseek");

		read = fread(cmp, 1, find_len, file_info.fptr);
		if (!read) {
			if (feof(file_info.fptr))
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

	if (fseek(file_info.fptr, orig_pos, SEEK_SET))
		perror("fseek");

	return (DM_OK);
}


int
dm_cmd_help()
{
	struct dm_help_rec	*h = help_recs;

	while (h->cmd != 0) {
		printf("%-15s   %s\n", h->cmd, h->descr);
		h++;
	}

	return (DM_OK);
}

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
		free(line);
		dm_update_prompt();
	}
	printf("\n");
}

int
dm_open_file(char *path)
{
	memset(&file_info, 0, sizeof(file_info));
	file_info.bits = 64; /* we guess */
	file_info.name = path;

	if ((file_info.fptr = fopen(path, "r")) == NULL) {
		DPRINTF(DM_D_ERROR, "Failed to open '%s': %s", path, strerror(errno));
		return (DM_FAIL);
	}

	if (fstat(fileno(file_info.fptr), &file_info.stat) < 0) {
		perror("fstat");
		return (DM_FAIL);
	}

	return (DM_OK);
}

void
dm_show_version()
{
	printf("%s\n", banner);
	printf("%-32s%s\n\n", "", "Version: " PACKAGE_VERSION);
}

void
usage()
{
	printf("Usage: dismantle [args] <elf binary>\n\n");
	printf("  Arguments:\n");
	printf("    -a         Disable ANSII colours\n");
	printf("    -x lvl     Set debug level to 'lvl'\n");
	printf("    -v         Show version and exit\n\n");
}

int
main(int argc, char **argv)
{
	int			ch, getopt_err = 0, getopt_exit = 0;

	while ((ch = getopt(argc, argv, "ahx:v")) != -1) {
		switch (ch) {
		case 'a':
			colours_on = 0;
			break;
		case 'x':
			dm_debug = atoi(optarg);
			if ((dm_debug < 0) || (dm_debug > 3))
				getopt_err = 0;
			break;
		case 'v':
			dm_show_version();
			getopt_exit = 1;
		case 'h':
			getopt_exit = 1;
			break;
		default:
			getopt_err = 1;
		}
	}

	/* command line args were bogus or we just need to exit */
	if ((getopt_err)  || (getopt_exit)) {
		if (getopt_err)
			DPRINTF(DM_D_ERROR, "Bogus usage!\n");

		usage();
		goto clean;
	}

	/* check a binary was supplied */
	if (argc == optind) {
		DPRINTF(DM_D_ERROR, "Missing filename\n");
		usage();
		goto clean;
	}

	/* From here on, cmd line was A-OK */
	if (dm_open_file(argv[optind]) != DM_OK)
		goto clean;

	/* parse elf and dwarf junk */
	dm_init_elf();
	dm_parse_pht();
	dm_parse_dwarf();

	ud_init(&ud);
	ud_set_input_file(&ud, file_info.fptr);
	ud_set_mode(&ud, file_info.bits);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	/* start at .text */
	if (file_info.elf)
		dm_seek(dm_find_section(".text"));
	else
		dm_seek(0);

	dm_show_version();
	dm_cmd_info(NULL);
	printf("\n");

	dm_interp();

	/* clean up */
clean:
	dm_clean_elf();
	dm_clean_dwarf();

	if (file_info.fptr != NULL)
		fclose(file_info.fptr);

	return (EXIT_SUCCESS);
}

int
dm_cmd_debug(char **args)
{
	int		lvl = atoi(args[0]);

	if ((lvl < 0) || (lvl > 3)) {
		DPRINTF(DM_D_ERROR, "Debug level is between 0 and 3");
		return (DM_FAIL);
	}

	dm_debug = lvl;

	return (DM_OK);
}

int
dm_cmd_debug_noargs(char **args)
{
	(void) args;

	printf("\n  %d (%s)\n\n", dm_debug, debug_names[dm_debug]);
	return (DM_OK);
}

int
dm_cmd_ansii(char **args)
{
	colours_on = atoi(args[0]);

	if (colours_on > 1)
		colours_on = 1;

	return (DM_OK);
}

int
dm_cmd_ansii_noargs(char **args)
{
	(void) args;
	printf("\n  %d\n\n", colours_on);
	return (DM_OK);
}
