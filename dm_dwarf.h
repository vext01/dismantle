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

#include <libdwarf.h>
#include <dwarf.h>

#include "dm_elf.h"
#include "tree.h"

/* a debug symbol */
struct dm_dwarf_sym_cache_entry {
	RB_ENTRY(dm_dwarf_sym_cache_entry)	 entry;
	char				*name;
	ADDR64				 vaddr;
	ADDR64				 offset;
	int				 sym_type;
	uint8_t				 offset_err; /* could not find offset */
};

int		dm_cmd_dwarf_funcs();
int		dm_dwarf_recurse_cu(Dwarf_Debug dbg);
int		dm_dwarf_recurse_die(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level);
int		get_die_and_siblings(Dwarf_Debug dbg,
		    Dwarf_Die in_die,int in_level);
int		dm_dwarf_sym_rb_cmp(struct dm_dwarf_sym_cache_entry *s1,
		    struct dm_dwarf_sym_cache_entry *s2);
int		dm_dwarf_inspect_die(Dwarf_Debug dbg, Dwarf_Die print_me, int level);
int		dm_parse_dwarf();
int		dm_clean_dwarf();
int		dm_dwarf_find_sym(char *name, struct dm_dwarf_sym_cache_entry **s);
int		dm_dwarf_find_sym_at_offset(ADDR64 off,
		    struct dm_dwarf_sym_cache_entry **ent);
