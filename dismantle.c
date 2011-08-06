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

#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "dm_dis.h"
#include "dm_elf.h"
#include "dm_cfg.h"
#include "dm_dom.h"
#include "dm_ssa.h"
#include "dm_dwarf.h"

struct stat			 bin_stat;
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


struct dm_cmd_sw {
	char			*cmd;
	uint8_t			 args;
	int			(*handler)(char **args);
} dm_cmds[] = {
	{"bits", 0, dm_cmd_bits_noargs},
	{"bits", 1, dm_cmd_bits},
	{"cfg", 0, dm_cmd_cfg},
	{"dis", 0, dm_cmd_dis_noargs},	{"pd", 0, dm_cmd_dis_noargs},
	{"dis", 1, dm_cmd_dis},		{"pd", 1, dm_cmd_dis},
	{"dom", 0, dm_cmd_dom},
	{"findstr", 1, dm_cmd_findstr}, {"/", 1, dm_cmd_findstr},
	{"funcs", 0, dm_cmd_dwarf_funcs}, {"f", 0, dm_cmd_dwarf_funcs},
	{"help", 0, dm_cmd_help},	{"?", 0, dm_cmd_help},
	{"hex", 0, dm_cmd_hex_noargs},  {"px", 0, dm_cmd_hex_noargs},
	{"hex", 1, dm_cmd_hex},         {"px", 1, dm_cmd_hex},
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
	{"/ str",               "Find ASCII string from current pos"},
	{"CTRL+D",		"Exit"},
	{"bits [set_to]",	"Get/set architecture (32 or 64)\n"},
	{"cfg",			"Show static CFG for current function"},
	{"dis/pd [ops]",	"Disassemble (8 or 'ops' operations)"},
	{"dom",			"Show dominance tree and frontiers of cur func"},
	{"funcs/f",		"Show functions from dwarf data"},
	{"help/?",		"Show this help"},
	{"hex/px [len]",        "Dump hex (64 or 'len' bytes)"},
	{"pht",			"Show program header table"},
	{"seek/s addr",		"Seek to an address"},
	{"sht",			"Show section header table"},
	{"ssa",			"Output SSA form of current function"},
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
	printf("\n");

	if (fseek(file_info.fptr, orig_pos, SEEK_SET) < 0) {
		perror("could not seek file");
		return (DM_FAIL);
	}

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
	NADDR                    byte = 0;
	char                    *find = args[0], *cmp = NULL;
	size_t                   find_len = strlen(find);
	size_t                   orig_pos = ftell(file_info.fptr), read;
	int                      ret = DM_FAIL;
	int                      hit = 0;

	if (bin_stat.st_size < (off_t) find_len) {
		fprintf(stderr, "file not big enough for that string\n");
		goto clean;
	}

	rewind(file_info.fptr);

	cmp = malloc(find_len);
	for (byte = 0; byte < bin_stat.st_size - find_len; byte++) {

#if 0
		printf("\r" NADDR_FMT "/" NADDR_FMT " (% 3d%%)", byte,
			(NADDR) (bin_stat.st_size - find_len),
			(byte / (float) (bin_stat.st_size - find_len) * 100));
#endif

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

	printf("\n");
	return (DM_OK);
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
		free(line); /* XXX not sure, roll with this for now */
		dm_update_prompt();
	}
	printf("\n");
}

int
dm_open_file(char *path)
{
	memset(&file_info, 0, sizeof(file_info));
	file_info.bits = 64; /* we guess */

	if ((file_info.fptr = fopen(path, "r")) == NULL) {
		perror("open");
		return (DM_FAIL);
	}

	if (fstat(fileno(file_info.fptr), &file_info.stat) < 0) {
		perror("fstat");
		return (DM_FAIL);
	}

	return (DM_OK);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: dismantle <elf binary>\n");
		exit(1);
	}

	if(dm_open_file(argv[1]) != DM_OK)
		goto clean;

	/* parse elf and dwarf junk */
	dm_init_elf();
	dm_parse_pht();
	dm_parse_dwarf();

	ud_init(&ud);
	ud_set_input_file(&ud, file_info.fptr);
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	/* start at .text */
	dm_seek(dm_find_section(".text"));

	dm_interp();

	/* clean up */
clean:
	dm_clean_elf();
	dm_clean_dwarf();

	if (file_info.fptr != NULL)
		fclose(file_info.fptr);

	return (EXIT_SUCCESS);
}
