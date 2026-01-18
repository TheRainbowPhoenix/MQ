//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/memory.h>
#include <mq/system/casiowin.h>
#include <mq/interfaces/keyboard.h>
#include <stdio.h>

// TODO[Casiowin GetKey*]: Emulate key buffer
// TODO[Casiowin GetKeyWait]: Emulate waiting modes (HALTON_TIMER{ON,OFF})

struct KeyMapping {
    i16 base, shift, alpha;
};

struct KeyMapping GetKeyKeymap[MQ_KEY__MAX] = {
    [MQ_KEY_F1]       = { 30009, 30009, 30009 },
    [MQ_KEY_F2]       = { 30010, 30010, 30010 },
    [MQ_KEY_F3]       = { 30011, 30011, 30011 },
    [MQ_KEY_F4]       = { 30012, 30012, 30012 },
    [MQ_KEY_F5]       = { 30013, 30013, 30013 },
    [MQ_KEY_F6]       = { 30014, 30014, 30014 },

    [MQ_KEY_SHIFT]    = { 30006, 30006, 30006 },
    [MQ_KEY_ALPHA]    = { 30007, 30007, 30007 },
    [MQ_KEY_EXIT]     = { 30002, 30029, 30002 },
    [MQ_KEY_MENU]     = { 30003, 30037, 30003 }, // BASE, ALPHA: Main menu
    [MQ_KEY_OPTN]     = { 30008,     0, 30008 },
    [MQ_KEY_VARS]     = { 30016, 30028, 30016 },
    [MQ_KEY_SETTINGS] = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_CATALOG]  = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_TOOLS]    = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_EXE]      = { 30004,  '\r', 30004 },
    [MQ_KEY_OK]       = {    -1,    -1,    -1 }, // TODO

    [MQ_KEY_UP]       = { 30018, 30052, 30018 },
    [MQ_KEY_DOWN]     = { 30023, 30053, 30023 },
    [MQ_KEY_LEFT]     = { 30020,    -1, 30020 }, // SHIFT: Contrast (on FX)
    [MQ_KEY_RIGHT]    = { 30018,    -1, 30018 }, // SHIFT: Contrast (on FX)
    [MQ_KEY_PREVTAB]  = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_NEXTTAB]  = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_PAGEUP]   = {    -1,    -1,    -1 }, // TODO
    [MQ_KEY_PAGEDOWN] = {    -1,    -1,    -1 }, // TODO

    [MQ_KEY_0]        = {   '0', 32592,   'Z' },
    [MQ_KEY_1]        = {   '1', 32593,   'U' },
    [MQ_KEY_2]        = {   '2', 32576,   'V' },
    [MQ_KEY_3]        = {   '3',     0,   'W' },
    [MQ_KEY_4]        = {   '4',    -1,   'P' }, // SHIFT: Catalog
    [MQ_KEY_5]        = {   '5',     0,   'Q' },
    [MQ_KEY_6]        = {   '6',     0,   'R' },
    [MQ_KEY_7]        = {   '7',    -1,   'M' }, // SHIFT: Capture
    [MQ_KEY_8]        = {   '8', 30050,   'N' },
    [MQ_KEY_9]        = {   '9', 30036,   'O' },
};

static bool GetKeyWait_bgsyscall(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    mqKeyboard *kbd = mach->keyboard;
    mqCpu *cpu = &mach->cpu;
    struct mqCasiowin_BGSyscallData_GetKeyWait *data =
        &Casiowin->bgs->GetKeyWait;
    struct mqCasiowin_GetKeyWaitArgs *args = &Casiowin->bgs->GetKeyWait.args;

    int keyNumber = mq_keyboard_getPressedKey(kbd);
    mqKeyboardKey const *info =
        (keyNumber >= 0) ? &kbd->keyInfo[keyNumber] : NULL;

    int col = -1, row = -1, keycode = 0;
    if(keyNumber == (int)kbd->onKey)
        col = row = 0;
    else if(keyNumber >= 0)
        col = info->col, row = info->row;

    /* Map from physical key IDs to GetKey() keycodes */
    // TODO: Handle all keys. Handle SHIFT, ALPHA.
    if(keyNumber >= 0 && info->keycode >= 0)
        keycode = GetKeyKeymap[info->keycode].base;

    // TODO: What is the return value when it's GetKey, not GetKeyWait?!

    /* Key presses are ignored until we get an idle keyboard at least once */
    bool keyEventAcceptable =
        (args->waitType == MQ_CASIOWIN_KEYWAIT_HALTOFF_TIMEROFF)
        || data->idleReached;
    /* ... do keep track whether that happened */
    data->idleReached |= (keyNumber < 0);

    /* A key event interrupts the call no matter the mode. */
    if(keyNumber >= 0 && keyEventAcceptable) {
        if(args->ptr_i32_col)
            mq_memory_write(mach, mach->memory, args->ptr_i32_col, 4, col + 1);
        if(args->ptr_i32_row)
            mq_memory_write(mach, mach->memory, args->ptr_i32_row, 4, row + 1);
        if(args->ptr_u16_key)
            mq_memory_write(mach, mach->memory, args->ptr_u16_key, 2, keycode);
        if(args->ptr_u32_key)
            mq_memory_write(mach, mach->memory, args->ptr_u32_key, 4, keycode);
        cpu->r[0] = MQ_CASIOWIN_KEYREP_KEYEVENT;
        // mq_log(MQ_LOG_WARNING, "GetKey: key obtained");
        return true;
    }

    /* If nothing happens... */

    /* HALTOFF_TIMEROFF: return instantly with KEYREP_NOEVENT */
    if(args->waitType == MQ_CASIOWIN_KEYWAIT_HALTOFF_TIMEROFF) {
        cpu->r[0] = MQ_CASIOWIN_KEYREP_NOEVENT;
        // mq_log(MQ_LOG_WARNING, "GetKey: nothing, skipped!");
        return true;
    }
    /* HALTON_TIMEROFF: wait */
    if(args->waitType == MQ_CASIOWIN_KEYWAIT_HALTON_TIMEROFF) {
        // mq_log(MQ_LOG_DEBUG, "GetKey: waiting...");
        return false;
    }
    /* HALTON_TIMERON: wait until time limit */
    if(args->waitType == MQ_CASIOWIN_KEYWAIT_HALTON_TIMERON) {
        mq_log(MQ_LOG_ERROR, "timed GetKeyWait isn't supported yet! o(x_x)o");
        mq_machine_setStuck(mach);
        return false;
    }

    mq_log(MQ_LOG_ERROR, "invalid GetKeyWait mode! o(x_x)o");
    mq_machine_setStuck(mach);
    return false;
}

void mq_casiowin_GetKey(mqMachine *mach, u32 ptr_u32_key)
{
    struct mqCasiowin_GetKeyWaitArgs args = {0};
    args.ptr_u32_key = ptr_u32_key;
    args.waitType = MQ_CASIOWIN_KEYWAIT_HALTON_TIMEROFF;
    return mq_casiowin_GetKeyWait(mach, args, true);
}

void mq_casiowin_GetKeyWait(
    mqMachine *mach, struct mqCasiowin_GetKeyWaitArgs args, bool isGetkey)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    Casiowin->bgs->GetKeyWait.args = args;
    Casiowin->bgs->GetKeyWait.idleReached = false;
    Casiowin->bgs->GetKeyWait.isGetkey = isGetkey;
    mq_casiowin_runBackgroundSyscall(mach, GetKeyWait_bgsyscall);
}
