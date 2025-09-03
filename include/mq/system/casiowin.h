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

    /* Address of the read-only "data area" which is where the emulator puts
       all the read-only OS data that needs to be accessed by address. */
    u32 rodataAreaAddress;
    u32 rodataAreaSize;
    /* Keymap, served by %1032 on FX and use for KeycodeToMatrixCode. May be
       NULL/0/-1 in which case there's no keymap. */
    int *rodataKeymap;
    int rodataKeymapSize;

    /* Same with the read-write data, such the VRAM and its copies. */
    u32 dataAreaAddress;
    u32 dataAreaSize;
    /* VRAM size. VRAM is always first in the data area, for consistency. */
    u32 dataVramSize;
    /* Number of VRAM buffers; can be up to 4 (for backups). */
    int dataVramCount;
};

/* Information that we keep track of in the machine structure. */
struct mqCasiowin {
    enum mqCasiowin_Version version;
    struct mqCasiowin_OSInfo const *info;

    /* Generated addresses for various data area values. */
    u32 rodataKeymapAddress;
    u32 dataVramAddresses[4];

    // TODO[mqCasiowin]: Localization, SH3/SH4 revision, version patch.

    /* Globals from the display system */

    /* VRAM buffer pointer. This points to a memory buffer, which means it's
       4-byte little-endian mode! */
    void *vramLE;
    /* Cursor position, starting at 0 (i.e. one less than Locate arguments) */
    int BdispCursorX;
    int BdispCursorY;
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

//=== Mono rendering functions ===============================================//
// These functions are used to serve syscalls. They can be called to emulate
// other high-level functions or larger syscalls.

/* Get and set individual pixels (out-of-bound is a constant 0). */
int mq_casiowin_mono_get_pixel(u8 *vramLE, uint x, uint y);
void mq_casiowin_mono_set_pixel(u8 *vramLE, uint x, uint y, int color);

/* Some common variants of the endless printing functions. */
void mq_casiowin_mono_Print(
    mqMachine *mach, u32 stringAddress, int maxX);
void mq_casiowin_mono_PrintMini(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode);
void mq_casiowin_mono_PrintXY(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode);

/* SaveDisp and RestoreDisp syscalls. */
void mq_casiowin_mono_SaveDisp(mqMachine *mach, int id);
void mq_casiowin_mono_RestoreDisp(mqMachine *mach, int id);

//=== RTC functions =========================================================//

/* RTC_GetTicks() - get RTC ticks */
bool mq_casiowin_rtc_getticks(mqMachine *mach, u32 *ret);

/* RTC_Reset() - reset the RTC */
bool mq_casiowin_rtc_reset(mqMachine *mach, u32 mode);

/* RTC_GetTime() - get time from RTC */
bool mq_casiowin_rtc_gettime(
    mqMachine *mach,
    u32 hour,
    u32 minutes,
    u32 second,
    u32 millisecond
);

MQ_END_DEFS
#endif /* MQ_SYSTEM_CASIOWIN_H */
