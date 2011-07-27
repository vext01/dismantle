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

FILE		*f;
struct stat	bin_stat;


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
	{"seek", 1, dm_cmd_seek},	{"s", 1, dm_cmd_seek},
	{"dis", 1, dm_cmd_dis},		{"pd", 1, dm_cmd_dis},
	{"dis", 0, dm_cmd_dis_noargs},	{"pd", 0, dm_cmd_dis_noargs},
	{"ssa", 1, dm_cmd_ssa},
	{"cfg", 0, dm_cmd_cfg},
	{"pht", 0, dm_cmd_pht},
	{"sht", 0, dm_cmd_sht},
	{"hex", 1, dm_cmd_hex},         {"px", 1, dm_cmd_hex},
	{"findstr", 1, dm_cmd_findstr}, {"/", 1, dm_cmd_findstr},
	{"hex", 0, dm_cmd_hex_noargs},  {"px", 0, dm_cmd_hex_noargs},
	{"help", 0, dm_cmd_help},	{"?", 0, dm_cmd_help},
	{NULL, 0, NULL}
};

struct dm_help_rec {
	char		*cmd;
	char		*descr;
} help_recs[] = {
	{"seek/s addr",		"Seek to an address"},
	{"dis/pd [ops]",	"Disassemble (8 or 'ops' operations)"},
	{"hex/px [len]",        "Dump hex (64 or 'len' bytes)"},
	{"pht",			"Show program header table"},
	{"sht",			"Show section header table"},
	{"/ str",               "Find ASCII string from current pos"},
	{"cfg",			"Output static CFG information for current function"},
	{"help/?",		"Show this help"},
	{"CTRL+D",		"Exit"},
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
	size_t		orig_pos = ftell(f);
	size_t		done = 0, read = 0, to_read = DM_HEX_CHUNK;
	uint8_t		buf[DM_HEX_CHUNK];

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
	size_t                   orig_pos = ftell(f), read;
	int                      ret = DM_FAIL;
	int                      hit = 0;

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

	/* XXX decide cleverly */
	ud_set_mode(&ud, 64);
	ud_set_syntax(&ud, UD_SYN_INTEL);

	/* start at .text */
	dm_seek(dm_find_section(".text"));

	dm_interp();

	return (EXIT_SUCCESS);
}
