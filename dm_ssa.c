#include "dm_ssa.h"

extern struct dm_instruction_se *instructions;

int
dm_cmd_ssa(char **args)
{
        struct          dm_ssa_index  *indices = NULL;
        unsigned int    read;
        char            *hex;
        int             ops = strtoll(args[0], NULL, 0), i;
        NADDR           addr = cur_addr;

        dm_ssa_index_init(&indices);
        dm_instruction_se_init();

        printf("\n");
        for (i = 0; i < ops; i++) {
                read = ud_disassemble(&ud);
                hex = ud_insn_hex(&ud);

                printf("    " NADDR_FMT ":  %-20s%s  ",
                    addr, hex, ud_insn_asm(&ud));

                addr = addr + read;

                if (instructions[ud.mnemonic].write)
                        indices[ud.operand[0].base].index++;

                printf("%d, ", indices[ud.operand[0].base].index);
                printf("%d, ", indices[ud.operand[1].base].index);
                printf("%d\n", indices[ud.operand[2].base].index);
        }

        dm_seek(cur_addr); /* rewind back */
        printf("\n");
        return (0);
}

void
dm_ssa_index_init(struct dm_ssa_index **indices)
{
        int                     i;

        *indices = malloc(sizeof(struct dm_ssa_index) * (UD_OP_CONST + 1));

        /* Initialise struct for SSA indexes */
        for (i = 0; i < UD_OP_CONST + 1; i++) {
                (*indices)[i].reg = (enum ud_type)i;
                (*indices)[i].index = 0;
        }
}

