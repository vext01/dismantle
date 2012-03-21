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

#ifndef __DM_ELF_H
#define __DM_ELF_H

#include <libelf/gelf.h>
#include <libelf/libelf.h>

#include "queue.h"
#include "common.h"

/* native address size */
#if defined(_M_X64) || defined(__amd64__)
	#define NADDR		Elf64_Addr
	#define ADDR_FMT_64	"0x%08lx"
	#define ADDR_FMT_32	"0x%08x"
	#define NADDR_FMT	ADDR_FMT_64
#else
	#define NADDR		Elf32_Addr
	#define ADDR_FMT_64    "0x%08llx"
	#define ADDR_FMT_32    "0x%08x"
	#define NADDR_FMT      ADDR_FMT_32
#endif
#define ADDR64			Elf64_Addr
#define ADDR32			Elf32_Addr

struct dm_pht_type {
	int		 type_int;
	char		*type_str;
	char		*descr;
};

/* XXX may aswell cache the entire Elf64_Phdr */
struct dm_pht_cache_entry {
	SIMPLEQ_ENTRY(dm_pht_cache_entry)	 entries;
	struct dm_pht_type			*type;
	ADDR64					 start_offset;
	ADDR64					 start_vaddr;
	ADDR64					 filesz;
	ADDR64					 memsz;
	int					 flags;
};

struct dm_pht_type	*dm_get_pht_info(int find);
NADDR			dm_find_section(char *find_sec);
NADDR			dm_find_size(char *find_sec);
int			dm_init_elf();
int			dm_make_pht_flag_str(int flags, char *ret);
int			dm_cmd_pht(char **args);
int			dm_cmd_sht(char **args);
int			dm_parse_pht();
int			dm_clean_elf();
int			dm_offset_from_vaddr(ADDR64 vaddr, ADDR64 *offset);
int			dm_cmd_offset(char **args);

#endif
