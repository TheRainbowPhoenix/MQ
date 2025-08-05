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
typedef struct mqMachine mqMachine;

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

/* Detailed information about each OS version. Unless otherwise specified, all
   addresses are set in P1. */
struct mqCasiowin_OSInfo {
    /* OS base and footer addresses */
    u32 OSBaseAddress;
    u32 OSFooterAddress;

    /* Version and date strings */
    char const *versionString;      /* MM.mm.pppp */
    char const *dateString;         /* YYYY.mmdd.hhmm */

    /* Syscall stub address; 0 if there is no syscall stub */
    u32 syscallStubAddress;
    // TODO[casiowin]: Syscall API version

    /* Heap address and size */
    u32 heapAddress;
    u32 heapSize;

    /* Address of the "data" area which is the fake area where the emulator
       puts all of the OS data that needs to be accessed by address. */
    u32 dataAreaAddress;
    u32 dataAreaSize;
    /* Keymap, served by %1032 on FX and use for KeycodeToMatrixCode. May be
       NULL/0/-1 in which case there's no keymap. */
    int *dataKeymap;
    int dataKeymapSize;
};

/* Information that we keep track of in the machine structure. */
struct mqCasiowin {
    enum mqCasiowin_Version version;
    struct mqCasiowin_OSInfo const *info;

    /* Generated addresses for various data area values. */
    u32 dataKeymapAddress;

    // TODO[mqCasiowin]: Localization, SH3/SH4 revision, version patch.

    /* Globals from the display system */
};

typedef enum mqCasiowin_Version mqCasiowin_Version;
typedef struct mqCasiowin_OSInfo mqCasiowin_OSInfo;
typedef struct mqCasiowin mqCasiowin;

/* Setup the CASIOWIN interface for the given OS version. */
bool mq_casiowin_setup(mqMachine *mach, mqCasiowin_Version version);

/* Get the CASIOWIN module for a machine, NULL if there's none. */
mqCasiowin *mq_casiowin_get(mqMachine *mach);

/* Get the static, constant OS information for a given version. This function
   does not require a machine or mqCasiowin instance. */
mqCasiowin_OSInfo const *mq_casiowin_getOSInfo(mqCasiowin_Version version);

/* Handle a syscall. */
void mq_casiowin_syscall(mqMachine *mach);

/* Initialize the system heap (emulated through gint's allocator). This
   function can be called explicitly but generally that's not required, as it
   will be initialized on-demand if a heap syscall is invoked. */
bool mq_casiowin_initHeap(mqMachine *mach);

MQ_END_DEFS
#endif /* MQ_SYSTEM_CASIOWIN_H */
