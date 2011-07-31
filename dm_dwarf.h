/* XXX License */

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
void		dm_dwarf_recurse_cu(Dwarf_Debug dbg);
void		dm_dwarf_recurse_die(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level);
void		get_die_and_siblings(Dwarf_Debug dbg,
		    Dwarf_Die in_die,int in_level);
int		dm_dwarf_sym_rb_cmp(struct dm_dwarf_sym_cache_entry *s1,
		    struct dm_dwarf_sym_cache_entry *s2);
int		dm_dwarf_inspect_die(Dwarf_Debug dbg, Dwarf_Die print_me, int level);
int		dm_parse_dwarf();
int		dm_clean_dwarf();
