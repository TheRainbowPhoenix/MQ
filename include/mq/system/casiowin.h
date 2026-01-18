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

/* Type of a background syscall progress function. */
typedef bool mq_casiowin_bgsyscall_t(struct mqMachine *mach);

/* Calculator series */
enum mqCasiowin_Series {
    MQ_CASIOWIN_SERIES_FX,
    MQ_CASIOWIN_SERIES_CG,
};

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
    /* OS series */
    int OSSeries;
    /* OS base and footer addresses */
    u32 OSBaseAddress;
    u32 OSFooterAddress;

    /* Version and date strings */
    char const *versionString;      /* MM.mm.pppp */
    char const *dateString;         /* YYYY.mmdd.hhmm */

    /* Syscall stub address; 0 if there is no syscall stub */
    u32 syscallStubAddress;
    // TODO[casiowin]: Syscall API version

    /* Addin loading address and size */
    u32 addinAddress;
    u32 addinSize;
    /* Heap address and size */
    u32 heapAddress;
    u32 heapSize;
    /* System Stack address and size */
    u32 systemStackAddress;
    u32 systemStackSize;
    /* User RAM address and size */
    u32 uramAddress;
    u32 uramSize;
    /* additional RAM address and size */
    u32 eramAddress;
    u32 eramSize;

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

/* Data tracked for background syscalls */
struct mqCasiowin_BGSyscallData;

/* Information that we keep track of in the machine structure. */
struct mqCasiowin {
    enum mqCasiowin_Version version;
    struct mqCasiowin_OSInfo const *info;

    /* Generated addresses for various data area values. */
    u32 rodataKeymapAddress;
    u32 dataVramAddresses[4];

    /* Currently-running blocking syscall. */
    mq_casiowin_bgsyscall_t *bgsyscall;

    /* Memory for background syscalls. */
    struct mqCasiowin_BGSyscallData *bgs;

    // TODO[mqCasiowin]: Localization, SH3/SH4 revision, version patch.

    //=== Globals from the display system ====================================//

    /* VRAM buffer pointer. This points to a memory buffer, which means it's
       4-byte little-endian mode! */
    void *vramLE;
    /* Cursor position, starting at 0 (i.e. one less than Locate arguments) */
    int BdispCursorX;
    int BdispCursorY;
};

typedef enum mqCasiowin_Version mqCasiowin_Version;
typedef struct mqCasiowin_OSInfo mqCasiowin_OSInfo;
typedef struct mqCasiowin_BGSyscallData mqCasiowin_BGSyscallData;
typedef struct mqCasiowin mqCasiowin;

/* Setup the CASIOWIN interface for the given OS version. */
bool mq_casiowin_setup(mqMachine *mach, mqCasiowin_Version version);

/* initialize CASIOWIN interface (e.g set registers' default value) */
void mq_casiowin_initialize(mqMachine *mach);

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

/* Start a background syscall. This is a syscall that blocks and may take an
   arbitrarily long time to run (e.g. GetKey). The background function will be
   called once immediately then regularly as a background process, and the
   syscall ends once the background function returns true. Currently it's only
   possible to run one background syscall at a time. If a background syscall
   needs to call another one, use the syscall support functions, which should
   offer the progress style. */
bool mq_casiowin_runBackgroundSyscall(
    mqMachine *mach, mq_casiowin_bgsyscall_t *bgsyscall);

//=== Mono rendering functions ===============================================//
// These functions are used to serve syscalls. They can be called to emulate
// other high-level functions or larger syscalls.

/* Get and set individual pixels (out-of-bound is a constant 0). */
int mq_casiowin_mono_get_pixel(u8 *vramLE, uint x, uint y);
void mq_casiowin_mono_set_pixel(u8 *vramLE, uint x, uint y, int color);

/* Get and set VRAM bytes. */
int mq_casiowin_mono_get_vram_byte(u8 *vramLE, uint xbyte, uint y);
void mq_casiowin_mono_set_vram_byte(u8 *vramLE, uint xbyte, uint y, int byte);

/* Base function for ShapeToVRAM, ShapeToDD etc. */
struct mqCasiowin_TShape;
struct mqCasiowin_TShapePixelInfo;
void mq_casiowin_mono_DrawShapePoint(
    u8 *vramLE, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);

/* Some common variants of the endless printing functions. */
void mq_casiowin_mono_Print(
    mqMachine *mach, u32 stringAddress, int maxX);
void mq_casiowin_mono_PrintMini(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode);
void mq_casiowin_mono_PrintXY(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode);

/* Display update. */
void mq_casiowin_mono_dupdate(mqMachine *mach);

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

//=== Syscall support functions ==============================================//
// These functions are used to serve syscalls on all APIs.

/** GetKeyWait **/

enum {
    MQ_CASIOWIN_KEYWAIT_HALTON_TIMEROFF = 0,
    MQ_CASIOWIN_KEYWAIT_HALTOFF_TIMEROFF = 1,
    MQ_CASIOWIN_KEYWAIT_HALTON_TIMERON = 2,
};
enum {
    MQ_CASIOWIN_KEYREP_NOEVENT = 0,
    MQ_CASIOWIN_KEYREP_KEYEVENT = 1,
    MQ_CASIOWIN_KEYREP_TIMEREVENT = 2,
};

struct mqCasiowin_GetKeyWaitArgs {
    /* Pointers to output col/row variables */
    u32 ptr_i32_col, ptr_i32_row;
    /* MQ_CASIOWIN_KEYWAIT_* enumerated value */
    int waitType;
    /* MQ_CASIOWIN_KEYREP_* enumerated value */
    int timeout;
    /* Menu setting: 0 allows return to menu (unless no waiting), 1 doesn't */
    int menu;
    /* Pointer to output keycode variable (two possible output sizes) */
    u32 ptr_u16_key;
    u32 ptr_u32_key;
};

void mq_casiowin_GetKey(mqMachine *mach, u32 ptr_u32_key);

void mq_casiowin_GetKeyWait(
    mqMachine *mach, struct mqCasiowin_GetKeyWaitArgs args, bool isGetkey);

/** Shape drawing functions: ShapeToVRAM, ShapeToDD, etc. **/

enum {
    /* Dot at x1, y1 */
    MQ_CASIOWIN_SHAPE_DOT = 1,
    /* Solid line from x1, y1 to x2, y2 */
    MQ_CASIOWIN_SHAPE_SOLID_LINE = 2,
    /* Dashed line alternating on_bits, off_bits, on_bits, etc. */
    MQ_CASIOWIN_SHAPE_ON_OFF_LINE = 3,
    /* Dashed line alterating off_bits, on_bits, off_bits, etc. */
    MQ_CASIOWIN_SHAPE_OFF_ON_LINE = 4,
    /* Rectangle from x1, y1 to x2, y2 */
    MQ_CASIOWIN_SHAPE_RECT = 5,
    /* Circle at x1, y1 with radius x2 */
    MQ_CASIOWIN_SHAPE_CIRCLE = 6,
};

enum {
    MQ_CASIOWIN_SHAPE_MODE_VRAM = 0x01,
    MQ_CASIOWIN_SHAPE_MODE_DD = 0x02,
};

struct mqCasiowin_TShape {
    u32 x1, y1, x2, y2;
    /* Always 2? */
    u8 const_2;
    /* Type of shape, see MQ_CASIOWIN_SHAPE_*. */
    u8 type;
    /* SimLo says: draw mode:
       1,1: set
       1,4: invert
       2,1: clear
       3;*: disables every second bit of the display (checkerboard)
       3,1: invert; checkerboard with (0,0) enabled
       3,2: set; checkerboard with (0,0) enabled
       3,3: clear; checkerboard with (0,0) enabled
       3,4: invert; checkerboard with (0,0) *disabled* */
    u8 f2, f3;
    /* Dash pattern for dashed lines */
    int on_bits, off_bits;
};

/* Structure for drawing pixels in DrawShape */
struct mqCasiowin_TShapePixelInfo {
    u8 mode;
    int x, y;
    int dash_counter;
};

void mq_casiowin_LineToVRAM(
    mqMachine *mach, int x1, int y1, int x2, int y2, int mode);
void mq_casiowin_ShapeToVRAM(
    mqMachine *mach, struct mqCasiowin_TShape const *shape);
void mq_casiowin_ShapeToDD(
    mqMachine *mach, struct mqCasiowin_TShape const *shape);
void mq_casiowin_ShapeToDDVRAM(
    mqMachine *mach, struct mqCasiowin_TShape const *shape);

void mq_casiowin_DrawShape(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);
void mq_casiowin_DrawShapePoint(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);
void mq_casiowin_DrawShapeLine(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);
void mq_casiowin_DrawShapeRect(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);
void mq_casiowin_DrawShapeCircle(
    mqMachine *mach, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape);

//=== All background syscall state ===========================================//

struct mqCasiowin_BGSyscallData {
    struct mqCasiowin_BGSyscallData_GetKeyWait {
        struct mqCasiowin_GetKeyWaitArgs args;
        bool idleReached;
        bool isGetkey;
    } GetKeyWait;
};

MQ_END_DEFS
#endif /* MQ_SYSTEM_CASIOWIN_H */
