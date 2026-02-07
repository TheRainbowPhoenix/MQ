//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/t6k11.h>
#include <mq/interfaces/display.h>
#include <mq/memory.h>
#include <mq/hooks.h>
#include <mq/mq.h>
#include <stdlib.h>

static int moduleID = -1;

static void inithook(void)
{
    moduleID = mq_module_register();
}
MQ_HOOK_REGISTER(init, inithook)

mqT6K11 *mq_t6k11_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void write_t6k11_select(mqT6K11 *T6K11, u8 value)
{
    T6K11->REG = value;
}

static void write_t6k11_data(mqMachine *mach, u32 value)
{
    mqT6K11 *T6K11 = mq_t6k11_get(mach);
    if(!T6K11)
        return;

    /* variant detection step.
     * try to detect if the application request the legacy T6K11 driver or
     * the "new" ML9801 one */
    if(T6K11->variant == T6K11_VARIANT_NONE) {
        if(T6K11->REG == 4 || T6K11->REG == 7)
            T6K11->variant = T6K11_VARIANT_T6K11;
        else
            T6K11->variant = T6K11_VARIANT_ML9801;
    }

    int reg = T6K11->REG + 100;
    if (T6K11->variant == T6K11_VARIANT_ML9801)
        reg = T6K11->REG + 200;
    switch(reg) {
        case 208:
        case 104: /* (T6K11) Y-address / X-address (depending on state) */
            bool setX = (value >> 7) & 1;
            if(setX)
                T6K11->row = value & 0x3f;
            else
                T6K11->col = (value & 0x1f) - ((reg == 208) ? 4 : 0);
            break;

        case 210:
        case 107: /* Data */
            if(T6K11->col >= 16 || T6K11->row >= 64) {
                mq_log(MQ_LOG_ERROR, "write_t6k11_data: out-of-bounds pixel "
                    "write at (col %d, row %d) ", T6K11->col, T6K11->row);
                mq_machine_setStuck(mach);
                return;
            }
            u8 *dst = (u8*)mach->display->data + 128 * T6K11->row + 8 * T6K11->col;
            for(int i = 0; i < 8; i++) {
                *dst++ = ~((i8)value >> 7);
                value <<= 1;
            }
            T6K11->col++;
            mq_display_setPixelsChanged(mach->display, true);
            if(T6K11->col >= 16 && T6K11->row == 63)
                mq_display_setFrameChanged(mach->display, true);
            break;

        default: {
            mq_log(MQ_LOG_DEBUG, "write_t6k11_data: REG: %02x, value: %08x "
                "(register not implemented)", T6K11->REG, value);
        }
    }
}

bool mq_t6k11_setup(mqMachine *mach)
{
    mqPage *pgb4000 = mq_memory_getPage(mach->memory, 0xb4000000);
    mqPage *pgb4010 = mq_memory_getPage(mach->memory, 0xb4010000);
    if(!pgb4000 || !pgb4010)
        return false;

    mqT6K11 *T6K11 = calloc(1, sizeof *T6K11);
    if(!T6K11)
        return false;
    T6K11->variant = T6K11_VARIANT_NONE;

    bool ok = true;
    ok &= mq_page_mapRegister8(pgb4000, "T6K11_SEL", 0xb4000000, NULL,
        write_t6k11_select, &T6K11->REG, T6K11);

    int ioID = mq_page_addIO(pgb4010, "T6K11_DATA",
        MQ_MMIO_SIZE_1 | MQ_MMIO_RELOC, NULL, write_t6k11_data, NULL, mach);
    ok &= mq_page_mapIO(pgb4010, ioID, 0xb4010000, 1, 1);

    if(ok)
        mach->modules[moduleID] = T6K11;
    else
        free(T6K11);

    return ok;
}

static void mq_t6k11_cleanup(mqMachine *mach)
{
    mqT6K11 *t6k11 = mach->modules[moduleID];
    if(t6k11)
        free(t6k11);
}
MQ_HOOK_REGISTER(module_cleanup, mq_t6k11_cleanup)

static void mq_t6k11_createObserver(mqMachine *omach, mqMachine const *mach)
{
    omach->modules[moduleID] = memdup(mach->modules[moduleID], sizeof(mqT6K11));
}
MQ_HOOK_REGISTER(module_createObserver, mq_t6k11_createObserver)

static void mq_t6k11_destroyObserver(mqMachine *omach)
{
    mqT6K11 *T6K11 = omach->modules[moduleID];
    if(T6K11)
        free(T6K11);
}
MQ_HOOK_REGISTER(module_destroyObserver, mq_t6k11_destroyObserver)
