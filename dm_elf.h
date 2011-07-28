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

struct dm_pht_type {
	int		 type_int;
	char		*type_str;
	char		*descr;
};

struct dm_pht_cache_entry {
	SIMPLEQ_ENTRY(dm_pht_cache_entry)	 entries;
	struct dm_pht_type			*type;
	NADDR					 start_offset;
	NADDR					 end_offset;
	NADDR					 start_vaddr;
	NADDR					 end_vaddr;
};

struct dm_pht_type	*dm_get_pht_info(int find);
NADDR			dm_find_section(char *find_sec);
int			dm_init_elf();
int			dm_make_pht_flag_str(int flags, char *ret);
int			dm_cmd_pht(char **args);
int			dm_cmd_sht(char **args);

#endif
