//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq-headless: Simple headless CLI interface for performance profiling

#include <mq/mq.h>
#include <mq/machine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void fatal(int rc, char const *fmt, ...);

int main(int argc, char **argv)
{
    if(argc != 2 || !strcmp(argv[0], "--help"))
        fatal(argc != 2, "usage: %s <add-in file>\n", argv[0]);

    char const *addinFile = argv[1];

    mq_init();
    mqMachine *mach = mq_machine_create();
    if(!mach)
        fatal(1, "could not allocate machine\n");

    mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
    if(!mq_machine_load_g3a(mach, addinFile))
        fatal(1, "could not load %s\n", addinFile);

    printf("Waiting 1 billion cycles...\n");
    mq_machine_cycle(mach, 1000*1000*1000);

    mq_machine_destroy(mach);
    mq_quit();
    return 0;
}

//============================================================================//

void fatal(int rc, char const *fmt, ...)
{
    char str[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(str, sizeof str, fmt, args);
    va_end(args);

    fputs(str, stderr);
    exit(rc);
}
