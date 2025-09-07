//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/memory.h>
#include <mq/system/casiowin.h>
#include <stdio.h>

// TODO[Casiowin GetKey*]: Emulate key buffer
// TODO[Casiowin GetKeyWait]: Emulate waiting modes (HALTON_TIMER{ON,OFF})

static bool GetKeyWait_bgsyscall(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    mqKeyboard *kbd = mach->keyboard;
    mqCpu *cpu = &mach->cpu;
    struct mqCasiowin_GetKeyWaitArgs *args = &Casiowin->bgs->GetKeyWait.args;

    int keyNumber = mq_keyboard_getPressedKey(kbd);
    int col = -1, row = -1;
    if(keyNumber == (int)kbd->onKey)
        col = row = 0;
    else if(keyNumber >= 0) {
        mqKeyboardKey const *info = &kbd->keyInfo[keyNumber];
        col = info->col;
        row = info->row;
    }

    /* In HALTOFF_TIMEROFF mode, we have to return instantly. */
    if(args->waitType == MQ_CASIOWIN_KEYWAIT_HALTOFF_TIMEROFF) {
        if(keyNumber >= 0) {
            mq_memory_write(mach, mach->memory, args->ptr_i32_col, 4, col + 1);
            mq_memory_write(mach, mach->memory, args->ptr_i32_row, 4, row + 1);
            cpu->r[0] = MQ_CASIOWIN_KEYREP_KEYEVENT;
        }
        else {
            cpu->r[0] = MQ_CASIOWIN_KEYREP_NOEVENT;
        }
        return true;
        }

    mq_log(MQ_LOG_ERROR, "long GetKeyWait isn't supported yet! o(x_x)o");
    mach->stuck = true;
    return false;
}

void mq_casiowin_GetKeyWait(
    mqMachine *mach, struct mqCasiowin_GetKeyWaitArgs args)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    Casiowin->bgs->GetKeyWait.args = args;
    mq_casiowin_runBackgroundSyscall(mach, GetKeyWait_bgsyscall);
}
