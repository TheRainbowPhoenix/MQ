//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.system.casiowin: Emulation of basic CASIOWIN constants
//
// This header defines the system API through which CASIOWIN's constants such
// as OS version, build date, syscall offset, etc. are provided.
//---

#ifndef MQ_SYSTEM_CASIOWIN_H
#define MQ_SYSTEM_CASIOWIN_H

#include <mq/defs.h>
#include <mq/system/casiowin.h>
MQ_START_DEFS

struct mqMachine;

/* Supported OS versions. */
enum mqCasiowin_Version {
    // MQ_CASIOWIN_FX100,  // Super old FX... (SH3)
    MQ_CASIOWIN_FX205,     // Old FX (both SH3 and SH4 exist)
    // MQ_CASIOWIN_FX300,  // G-III/35+E II (SH4 only)
    // MQ_CASIOWIN_CG200,  // Prizm fx-CG 10/20
    MQ_CASIOWIN_CG380,
    // MQ_CASIOWIN_MP100,  // Math+ 1.00
    // MQ_CASIOWIN_MP200,  // Math+ 2.00
};

/* Information that we keep track of in the machine structure. */
struct mqCasiowin {
    enum mqCasiowin_Version version;
    // TODO[mqCasiowin]: Localization, SH3/SH4 revision, version patch.

    /* Strings which get read from different offsets. Not NUL-terminated! */
    char str_version[10];
    char str_serial[8];
    char str_date[14];
};

typedef struct mqCasiowin mqCasiowin;

/* Setup the CASIOWIN interface for the given OS version. */
bool mq_casiowin_setup(struct mqMachine *mach, enum mqCasiowin_Version version);

/* Get the CASIOWIN module for a machine, NULL if there's none. */
mqCasiowin *mq_casiowin_get(struct mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_SYSTEM_CASIOWIN_H */
