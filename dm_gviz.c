#define _GNU_SOURCE
#include <stdlib.h>
#include "dm_gviz.h"

FILE*
dm_new_graph(char *filename)
{
	FILE *fp = fopen(filename, "w+");
	if (fp) fprintf(fp, "digraph g {\n\tnode [shape = \"box\"];\n");
	return fp;
}

void dm_add_edge(FILE *fp, char *source, char *dest)
{
	fprintf(fp, "\t\"%s\" -> \"%s\";\n", source, dest);
}

void dm_add_label(FILE *fp, char *node, char *label)
{
	fprintf(fp, "\t\"%s\" [label=\"%s\"];\n", node, label);
}

void dm_end_graph(FILE *fp)
{
	fprintf(fp, "}\n");
	fclose(fp);
}

void dm_display_graph(char *filename)
{
	char *sys_buf = NULL;
	if (asprintf(&sys_buf, "dot -Txlib %s &", filename) != -1)
		system(sys_buf);
}

