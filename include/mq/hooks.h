//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.hooks: Runtime modules and extensions
//
// This header defines the hooks used by MQ to register hardware modules, calc
// models, and other details at runtime. It makes it more convenient to add new
// definitions in their own files without a centralized listing.
//
// Note that hooks are registered in global constructors but that's just a data
// structure manipulation. It can be done in any order and does not require any
// setup. The code that runs before main() just fills some arrays of functions
// to be called later, and doesn't execute anything.
//---

#ifndef MQ_CORE_HOOKS_H
#define MQ_CORE_HOOKS_H

#include <mq/defs.h>
MQ_START_DEFS

struct mqMachine;
struct mqMemory;

/* [init] Hook that runs in mq_init(). */
typedef void mq_hook_init_t(void);
bool mq_hook_init(mq_hook_init_t *function);
void mq_callhook_init(void);

/* [quit] Hook that runs in mq_quit(). */
typedef void mq_hook_quit_t(void);
bool mq_hook_quit(mq_hook_quit_t *function);
void mq_callhook_quit(void);

/* [module_createObserver] Hook that runs when creating a machine observer. */
typedef void mq_hook_module_createObserver_t(
    struct mqMachine *omach, struct mqMachine const *mach);
bool mq_hook_module_createObserver(mq_hook_module_createObserver_t *function);
void mq_callhook_module_createObserver(
    struct mqMachine *omach, struct mqMachine const *mach);

/* [module_cleanup] Hook that runs when an mqMachine cleans up module data. */
typedef void mq_hook_module_cleanup_t(struct mqMachine *mach);
bool mq_hook_module_cleanup(mq_hook_module_cleanup_t *function);
void mq_callhook_module_cleanup(struct mqMachine *mach);

/* [module_destroyObserver]
   Hook that runs when destroying a machine observer. */
typedef void mq_hook_module_destroyObserver_t(struct mqMachine *omach);
bool mq_hook_module_destroyObserver(mq_hook_module_destroyObserver_t *function);
void mq_callhook_module_destroyObserver(struct mqMachine *omach);

/* [memory_read] hook that runs when there is an uncaught memory read in a
   configurable memory area. If multiple hooks cover the same address one of
   them will be used but which is unspecified. */
typedef bool mq_hook_memory_read_t(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 *res);
bool mq_hook_memory_read(mq_hook_memory_read_t *function);
bool mq_callhook_memory_read(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 *res);

/* [memory_write] hook that runs when there is an uncaught memory write in a
   configurable memory area. If multiple hooks cover the same address one of
   them will be used but which is unspecified. */
typedef bool mq_hook_memory_write_t(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 value);
bool mq_hook_memory_write(mq_hook_memory_write_t *function);
bool mq_callhook_memory_write(
    struct mqMachine *mach, struct mqMemory *mem, u32 addr, int sz, u32 value);

/* Register a hook with an anonymous constructor */
#define MQ_HOOK_REGISTER(NAME, FUNCTION) MQ_HKR2(NAME, __COUNTER__, FUNCTION)
#define MQ_HKR2(NAME, COUNTER, FUNCTION) MQ_HKR3(NAME, COUNTER, FUNCTION)
#define MQ_HKR3(NAME, COUNTER, FUNCTION) \
    __attribute__((constructor)) static void _hook_ctor##COUNTER(void) { \
        mq_hook_##NAME(FUNCTION); \
    }

MQ_END_DEFS
#endif /* MQ_CORE_HOOKS_H */
