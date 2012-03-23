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

#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dm_elf.h"
#include "tree.h"

#define PACKAGE_VERSION		"0.1"

/* ANSII  Foreground Colours */
#define ESCAPE	"\033["

#define NORMAL	"0;"
#define LIGHT	"1;"
#define DARK	"2;"

#define INVERT	"7;"
#define BLINK	"5;"
#define USCORE	"4;"
#define HIDDEN	"8;"

#define FG	"3"
#define BG	"4"

#define BLACK	"0m"
#define	RED	"1m"
#define GREEN	"2m"
#define YELLOW	"3m"
#define BLUE	"4m"
#define Magenta "5m"
#define CYAN	"6m"
#define WHITE	"7m"

#define RESET	"0m"
#define END	"m"

extern uint8_t			colours_on;
#define ANSII_BLACK		(colours_on ? "\033[0;30m" : "")
#define ANSII_RED               (colours_on ? "\033[0;31m" : "")
#define ANSII_GREEN             (colours_on ? "\033[0;32m" : "")
#define ANSII_BROWN		(colours_on ? "\033[0;33m" : "")
#define ANSII_BLUE		(colours_on ? "\033[0;34m" : "")
#define ANSII_MAGENTA		(colours_on ? "\033[0;35m" : "")
#define ANSII_CYAN		(colours_on ? "\033[0;36m" : "")
#define ANSII_GRAY		(colours_on ? "\033[0;37m" : "")

#define ANSII_DARKGRAY		(colours_on ? "\033[01;30m" : "")
#define ANSII_LIGHTRED		(colours_on ? "\033[01;31m" : "")
#define ANSII_LIGHTGREEN	(colours_on ? "\033[01;32m" : "")
#define ANSII_YELLOW            (colours_on ? "\033[01;33m" : "")
#define ANSII_LIGHTBLUE		(colours_on ? "\033[01;34m" : "")
#define ANSII_LIGHTMAGENTA	(colours_on ? "\033[01:35m" : "")
#define ANSII_LIGHTCYAN		(colours_on ? "\033[01;36m" : "")
#define ANSII_REALWHITE		(colours_on ? "\033[01;37m" : "")

#define ANSII_WHITE             (colours_on ? "\033[0m" : "")


/* simple debug facility (originally from HGD code) */
extern int			 dm_debug;
extern char			*debug_names[];
#define DM_D_ERROR		0
#define DM_D_WARN		1
#define DM_D_INFO		2
#define DM_D_DEBUG		3

#define DPRINTF(level, x...)						\
	do {								\
		if (level <= dm_debug) {				\
			fprintf(stderr, "[%s - %08d %s:%s():%d]\n\t",	\
			    debug_names[level], getpid(),		\
			    __FILE__, __func__, __LINE__);		\
			fprintf(stderr, x);				\
			fprintf(stderr, "\n");				\
		}							\
	} while (0)

struct dm_file_info {
	char		*name;
	char		*ident;
	FILE		*fptr;
	uint8_t		elf;
	uint8_t		dwarf;
	uint8_t		bits; /* 32 or 64 binary */
	struct stat	stat;
};

struct dm_setting {
	RB_ENTRY(dm_setting)	 entry;
	char			*name;
	int			 type;
#define DM_SETTING_INT		(0)
#define DM_SETTING_STR		(1)
	union dm_setting_val {
		int		 ival;
		char		*sval;
	} val;
	char			*help;
};

extern struct dm_file_info	file_info;

#define DM_OK		0
#define DM_FAIL		-1

#define DM_RULE \
"----------------------------------------------------------------------------"

extern FILE	*f;

int dm_find_setting(char *name, struct dm_setting **s);

#endif
