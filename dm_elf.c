#include "dm_elf.h"

Elf	*elf = NULL;

struct dm_pht_type pht_types[] = {
	{PT_NULL,		"PT_NULL",		"Unused"},
	{PT_LOAD,		"PT_LOAD",		"Loadable segment"},
	{PT_DYNAMIC,		"PT_DYNAMIC",		"Dynamic linking info"},
	{PT_INTERP,		"PT_INTERP",		"Interpreter field"},
	{PT_NOTE,		"PT_NOTE",		"Auxillary info"},
	{PT_SHLIB,		"PT_SHLIB",		"Non-standard"},
	{PT_PHDR,		"PT_PHDR",		"PHT size"},
	{PT_TLS,		"PT_TLS",		"Thread local storage"},
	{PT_LOOS,		"PT_LOOS",		"System specific (lo/start mark)"},
	{PT_HIOS,		"PT_HIOS",		"System specific (hi/end mark)"},
	{PT_LOPROC,		"PT_LOPROC",		"CPU specific (lo/start mark)"},
	{PT_HIPROC,		"PT_HIPROC",		"CPU system-specific (hi/end mark)"},
/* XXX define thses for !glibc systems */
#ifdef __LINUX__
	{PT_GNU_EH_FRAME, 	"PT_GNU_EH_FRAME",	"GCC .eh_frame_hdr segment"},
	{PT_GNU_STACK,		"PT_GNU_STACK",		"Indicates stack executability"},
	{PT_GNU_RELRO,		"PT_GNU_RELRO",		"Read-only after relocation"},
	{PT_LOSUNW,		"PT_LOSUNW",		"Sun specific (lo/start mark)"},
	{PT_SUNWBSS,		"PT_SUNWBSS",		"Sun specific segment"},
	{PT_SUNWSTACK,		"PT_SUNWSTACK",		"Sun stack segment"},
	{PT_HISUNW,		"PT_HISUNW",		"Sun specific (hi/end mark)"},
#endif
	{-1,			NULL,			NULL},
};

struct dm_pht_type	unknown_pht_type = {-1, "???", "Unknown PHT type"};

struct dm_pht_type *
dm_get_pht_info(int find)
{
	struct dm_pht_type		*t = pht_types;

	while (t->type_int != -1) {
		if (t->type_int == find)
			break;
		t++;
	}

	if (t->type_int == -1)
		return (NULL);

	return (t);
}

/*
 * get the offset of a section name
 */
NADDR
dm_find_section(char *find_sec)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	NADDR		 ret = -1;

	if (elf == NULL)
		goto clean;

	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	sec = NULL ;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		if (gelf_getshdr(sec, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr: %s", elf_errmsg(-1));
			goto clean;
		}

		if ((sec_name =
		    elf_strptr(elf, shdrs_idx, shdr.sh_name)) == NULL) {
			fprintf(stderr, "elf_strptr: %s", elf_errmsg(-1));
			goto clean;
		}

		if (strcmp(sec_name, find_sec) == 0) {
			ret = shdr.sh_offset;
			break;
		}
	}

clean:
	return (ret);
}

int
dm_init_elf()
{
	Elf_Kind		 ek;

	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "elf_version: %s\n", elf_errmsg(-1));
		goto err;
	}

	if ((elf = elf_begin(fileno(f), ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "elf_begin: %s\n", elf_errmsg(-1));
		goto err;
	}

	ek = elf_kind(elf);

	if (ek != ELF_K_ELF) {
		fprintf(stderr, "Does not appear to have an ELF header\n");
		goto err;
	}

	return (0);
err:
	elf_end(elf);
	elf = NULL;

	return (-1);
}

/*
 * make a RWX string for program header flags
 * ret is a preallocated string of atleast 4 in length
 */
#define DM_APPEND_FLAG(x)	*p = (x); p++;
int
dm_make_pht_flag_str(int flags, char *ret)
{
	char			*p = ret;

	memset(ret, 0, 4);

	if (flags & PF_R)
		DM_APPEND_FLAG('R');

	if (flags & PF_W)
		DM_APPEND_FLAG('W');

	if (flags & PF_X)
		DM_APPEND_FLAG('X');

	return (DM_OK);
}

/*
 * show the program header table
 */
int
dm_cmd_pht(char **args)
{
	int			 ret = DM_FAIL;
	GElf_Phdr		 phdr;
	size_t			 num_phdrs, i;
	char			 flags[4];
	struct dm_pht_type	*pht_t;
	
	(void) args;	

	if (elf == NULL)
		goto clean;

	if (elf_getphdrnum(elf, &num_phdrs) != 0) {
		fprintf(stderr, "elf_getphdrnum: %s", elf_errmsg ( -1));
		goto clean;
	}

	/* Get program header table */
	printf("\nFound %lu program header records:\n", num_phdrs);
	printf("%s\n", DM_RULE);
	printf("%-10s | %-10s | %-5s | %-20s | %-20s\n",
	    "Offset", "Virtual", "Flags", "Type", "Description");
	printf("%s\n", DM_RULE);

	for (i = 0; i < num_phdrs; i++) {

		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			fprintf(stderr, "elf_getphdr: %s", elf_errmsg(-1));
			goto clean;
		}

		dm_make_pht_flag_str(phdr.p_flags, flags);
		pht_t = dm_get_pht_info(phdr.p_type);

		if (!pht_t)
			pht_t = &unknown_pht_type;

		/* offset is ElfAddr_64 bit on every arch for some reason */
		printf(ADDR_FMT_64 " | " ADDR_FMT_64 " | %-5s | %-10s | %-20s",
			phdr.p_offset, phdr.p_vaddr, flags,
			pht_t->type_str, pht_t->descr);


		printf("\n");
	}
	ret = DM_OK;
	printf("%s\n", DM_RULE);
clean:
	printf("\n");
	return (ret);
}

/*
 * dump the section headers
 */
int
dm_cmd_sht(char **args)
{
	Elf_Scn			*sec;
	size_t			 shdrs_idx;
	GElf_Shdr		 shdr;
	char			*sec_name;
	int			 ret = DM_FAIL;

	(void) args;

	if (elf == NULL)
		goto clean;

	/* Get section header table */
	if (elf_getshdrstrndx(elf, &shdrs_idx) != 0) {
		fprintf(stderr, "elf_getshdrsrtndx: %s", elf_errmsg(-1));
		goto clean;
	}

	printf("\nFound %lu section header records:\n", shdrs_idx);
	printf("%s\n", DM_RULE);
	printf("%-20s | %-10s | %-10s\n", "Name", "Offset", "Virtual");
	printf("%s\n", DM_RULE);

	sec = NULL ;
	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		if (gelf_getshdr(sec, &shdr) != &shdr) {
			fprintf(stderr, "gelf_getshdr: %s", elf_errmsg(-1));
			goto clean;
		}

		if (!(sec_name = elf_strptr(elf, shdrs_idx, shdr.sh_name))) {
			fprintf(stderr, "elf_strptr: %s", elf_errmsg(-1));
			goto clean;
		}
		printf("%-20s | " ADDR_FMT_64 " | " NADDR_FMT "\n",
		    sec_name, shdr.sh_offset, (NADDR) shdr.sh_addr);
	}
	printf("%s\n", DM_RULE);

	ret = DM_OK;
clean:
	printf("\n");
	return (ret);
}
