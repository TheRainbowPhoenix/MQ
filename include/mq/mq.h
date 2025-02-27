//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// Main library header

#ifndef MQ_MQ_H
#define MQ_MQ_H

#include <mq/defs.h>
MQ_START_DEFS

/* Initialize the emulator library.
   TODO: Load extensions here. */
void mq_init(void);

/* Shutdown the emulator library. */
void mq_quit(void);

/* Register a hardware module ID. The peripheral module can only be used by
   machines created after this function is called. Therefore, this should
   always be called in the init hook. */
int mq_module_register(void);

/* Get the number of currently-registered hardware modules. */
int mq_module_count(void);

/* Register a hardware process ID. This can be used to set a hardware process
   function on every mqMachine and dynamically update it. This should always be
   called in the init hook. */
int mq_process_register(void);

/* Get the number of currently-registered hardware processes. */
int mq_process_count(void);

MQ_END_DEFS
#endif /* MQ_MQ_H */
