/*
  Copyright (c) 2011 Edd Barrett. All rights reserved.

  This code was based upon the simplereader.c example distrubuted with
  dwarfutils. The original copyright and license follows.

  Copyright (c) 2009-2010	David Anderson.	All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the example nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY David Anderson ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL David Anderson BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <libdwarf.h>
#include <dwarf.h>

#include "dm_dwarf.h"

extern FILE			*f;

int
dm_dwarf_funcs()
{
	Dwarf_Debug			dbg = 0;
	Dwarf_Error			error;
	Dwarf_Handler			errhand = 0;
	Dwarf_Ptr			errarg = 0;

	if (dwarf_init(fileno(f), DW_DLC_READ, errhand,
		    errarg, &dbg, &error) != DW_DLV_OK) {
		printf("Giving up, cannot do DWARF processing\n");
		exit(1);
	}

	read_cu_list(dbg);

	if (dwarf_finish(dbg,&error) != DW_DLV_OK)
		fprintf(stderr, "dwarf_finish failed!\n");

	return (0); /* XXX */
}

static void
read_cu_list(Dwarf_Debug dbg)
{
	Dwarf_Unsigned		cu_header_length = 0;
	Dwarf_Half		version_stamp = 0;
	Dwarf_Unsigned		abbrev_offset = 0;
	Dwarf_Half		address_size = 0;
	Dwarf_Unsigned		next_cu_header = 0;
	Dwarf_Error		error;
	int			cu_number = 0, res;
	Dwarf_Die		no_die = 0;
	Dwarf_Die		cu_die = 0;

	for(;;++cu_number) {

		no_die = cu_die = 0;
		res = DW_DLV_ERROR;

		res = dwarf_next_cu_header(dbg,&cu_header_length,
		    &version_stamp, &abbrev_offset, &address_size,
		    &next_cu_header, &error);

		printf("Offset: %llu\n", abbrev_offset);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_next_cu_header\n");
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY)
			return; /* done */

		/* The CU will have a single sibling, a cu_die. */
		res = dwarf_siblingof(dbg,no_die,&cu_die,&error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof on CU die \n");
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY) { /* Impossible case. */
			printf("no entry! in dwarf_siblingof on CU die \n");
			exit(1);
		}

		get_die_and_siblings(dbg, cu_die, 0);
		dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
	}
}

static void
get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level)
{
	int			res = DW_DLV_ERROR;
	Dwarf_Die		cur_die = in_die, child = 0, sib_die = 0;
	Dwarf_Error		error;

	print_die_data(dbg, in_die,in_level);

	for (;;) {
		sib_die = 0;

		res = dwarf_child(cur_die,&child,&error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_child, level %d\n",in_level);
			exit(1);
		}

		if (res == DW_DLV_OK)
			get_die_and_siblings(dbg,child,in_level+1);

		res = dwarf_siblingof(dbg, cur_die, &sib_die, &error);
		if (res == DW_DLV_ERROR) {
			printf("Error in dwarf_siblingof, level %d\n",in_level);
			exit(1);
		}

		if (res == DW_DLV_NO_ENTRY)
			break;	/* Done at this level. */

		if (cur_die != in_die)
			dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);

		cur_die = sib_die;
		print_die_data(dbg, cur_die, in_level);
	}

	return;
}

static int
print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level)
{
	char			*name = 0;
	Dwarf_Error		 error = 0;
	Dwarf_Half		 tag = 0;
	const char		*tagname = 0;
	int			 res = dwarf_diename(print_me,&name,&error);
	Dwarf_Addr		 lo;

	if (res == DW_DLV_ERROR) {
		printf("Error in dwarf_diename , level %d \n",level);
		exit(1);
	}

	if (res == DW_DLV_NO_ENTRY)
		return 0;

	res = dwarf_tag(print_me, &tag, &error);
	if (res != DW_DLV_OK) {
		printf("Error in dwarf_tag , level %d \n",level);
		exit(1);
	}

	/* only functions */
	if (tag != DW_TAG_subprogram)
		return 1;

	/* get virtual addr */
	res = dwarf_lowpc(print_me, &lo, &error);
	if (res != DW_DLV_OK) {
		fprintf(stderr, "Failed to dwarf_lopc()");
		if (res == DW_DLV_NO_ENTRY)
			fprintf(stderr, "no entry\n");
		exit(1);
	}

	/* get the function name */
	res = dwarf_get_TAG_name(tag,&tagname);
	if (res != DW_DLV_OK) {
		fprintf(stderr, "Failed to dwarf_get_TAG_name");
		exit(1);
	}

	printf("<%d> tag: %d %s  name: %s  addr: %llu\n",
	    level, tag, tagname, name, lo);

	dwarf_dealloc(dbg,name,DW_DLA_STRING);

	return (1);
}
