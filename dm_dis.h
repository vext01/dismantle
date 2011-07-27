#ifndef __DM_DIS_H
#define __DM_DIS_H

#include <udis86.h>
#include "common.h"

extern ud_t ud;
extern NADDR cur_addr;

int dm_seek(NADDR addr);
int dm_cmd_seek(char **args);
int dm_disasm_op(NADDR addr);
int dm_cmd_dis(char **args);
int dm_cmd_dis_noargs(char **args);

#endif
