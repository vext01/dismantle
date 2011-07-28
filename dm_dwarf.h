/* XXX License */

#include <libdwarf.h>
#include <dwarf.h>

int		dm_cmd_dwarf_funcs();
void		read_cu_list(Dwarf_Debug dbg);
void		get_die_and_siblings(Dwarf_Debug dbg,
		    Dwarf_Die in_die,int in_level);
int		print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level);
