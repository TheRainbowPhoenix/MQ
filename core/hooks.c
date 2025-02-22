//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/hooks.h>

#define MQ_DEFINE_HOOK_2(TYPE, ARRAY, COUNT, MAXCOUNT, HOOKFUN) \
    static TYPE *ARRAY[MAXCOUNT+1] = {NULL};    \
    static int COUNT = 0;                       \
    bool HOOKFUN(TYPE *function) {              \
        if(COUNT >= MAXCOUNT) return false;     \
        ARRAY[COUNT++] = function;              \
        return true;                            \
    }

#define MQ_DEFINE_HOOK(NAME, MAXCOUNT) \
    MQ_DEFINE_HOOK_2(mq_hook_##NAME##_t, hk_##NAME, hk_##NAME##_count, \
                     MAXCOUNT, mq_hook_##NAME)

//============================================================================//

MQ_DEFINE_HOOK(init, 16)
void mq_callhook_init(void)
{
    for(int i = 0; hk_init[i]; i++)
        hk_init[i]();
}

MQ_DEFINE_HOOK(quit, 16)
void mq_callhook_quit(void)
{
    for(int i = 0; hk_quit[i]; i++)
        hk_quit[i]();
}

MQ_DEFINE_HOOK(module_cleanup, 32)
void mq_callhook_module_cleanup(struct mqMachine *mach)
{
    for(int i = 0; hk_module_cleanup[i]; i++)
        hk_module_cleanup[i](mach);
}

MQ_DEFINE_HOOK(memory_read, 32)
bool mq_callhook_memory_read(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 *res)
{
    for(int i = 0; hk_memory_read[i]; i++) {
        if(hk_memory_read[i](mach, mem, addr, sz, res))
            return true;
    }
    return false;
}

MQ_DEFINE_HOOK(memory_write, 32)
bool mq_callhook_memory_write(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 value)
{
    for(int i = 0; hk_memory_write[i]; i++) {
        if(hk_memory_write[i](mach, mem, addr, sz, value))
            return true;
    }
    return false;
}
