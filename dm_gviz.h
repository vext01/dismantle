#include <stdio.h>

FILE* dm_new_graph(char *filename);
void dm_add_edge(FILE *fp, char *source, char *dest);
void dm_add_label(FILE *fp, char *node, char *label);
void dm_end_graph(FILE *fp);
void dm_display_graph(char *filename);
