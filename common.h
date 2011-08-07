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

/* ANSII colours */
extern uint8_t			colours_on;
#define ANSII_YELLOW            (colours_on ? "\033[33m" : "")
#define ANSII_RED               (colours_on ? "\033[31m" : "")
#define ANSII_GREEN             (colours_on ? "\033[32m" : "")
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

extern struct dm_file_info	file_info;

#define DM_OK		0
#define DM_FAIL		-1

#define DM_RULE \
"----------------------------------------------------------------------------"

extern FILE	*f;

#endif
