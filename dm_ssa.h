#include "common.h"
#include "dm_dom.h"
#include "dm_cfg.h"

struct dm_ssa_index {
        enum ud_type                    reg;
        int                             index;
};

void            dm_ssa_index_init(struct dm_ssa_index **indices);
int             dm_cmd_ssa(char **args);
