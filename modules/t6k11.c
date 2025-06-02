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

static void write_t6k11_data(mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqT6K11 *T6K11 = mach->modules[moduleID];
    if(!T6K11)
        return;
    (void)addr;
    (void)size;

    // mq_log(MQ_LOG_DEBUG, "write_t6k11_data: REG: %02x, value: %08x", T6K11->REG, value);

    switch(T6K11->REG) {
        // R0: Display Mode (DPE)
        // R2: Set Analog Control Mode (APE)
        // R3: Set alternating signal mode (APE)
        // R5: Set Z-address (SZE)
        // R6: Contrast Control (SCE)
        // R12: D/A converter power control (OPC)

        // R1: Counter Mode (CSE)
        case 1:
            break; // Not implemented

        // R4: Set Y-address (SYE) / Set X-address (SXE)
        case 4:
            bool setX = (value >> 7) & 1;
            if (setX) {
                T6K11->row = value & 0x3f;
            }
            else {
                T6K11->col = value & 0x1f;
            }
            break;
        // R7: Data Write (DAWR) / Data Read (DARD)
        case 7:
            for (int i = 0; i < 8; i++) {
                int b = (value >> (7-i)) & 1;
                int x = T6K11->col * 8 + i;
                int y = T6K11->row;
                if (x > 127 || y > 63) {
                    mq_log(MQ_LOG_ERROR, "write_t6k11_data: pixel at (%d, %d) is out of bounds", x, y);
                    mach->stuck = true;
                    break;
                }
                ((u8*)(mach->display->data))[y * 128 + x] = b ? 0x00 : 0xff;
            }
            T6K11->col++;
            mqDisplay_setDirty(mach->display, true);
            break;
        default:
            mq_log(MQ_LOG_DEBUG, "write_t6k11_data: REG: %02x, value: %08x (register not implemented)", T6K11->REG, value);
            break;
    }
}

bool mq_t6k11_setup(mqMachine *mach)
{
    mqPage *pgb4000 = mq_memory_getPage(mach->memory, 0xb4000000); // Select register
    mqPage *pgb4010 = mq_memory_getPage(mach->memory, 0xb4010000); // Data register
    if(!pgb4000 || !pgb4010)
        return false;

    mqT6K11 *T6K11 = calloc(1, sizeof *T6K11);
    if(!T6K11)
        return false;

    bool ok = true;
    ok &= mq_page_mapRegister8(pgb4000, "t6k11_sel", 0xb4000000, NULL, 
        write_t6k11_select, &T6K11->REG, T6K11);

    int ioID = mq_page_addIO(pgb4010, "t6k11_data", MQ_MMIO_SIZE_1, NULL, 
        write_t6k11_data, NULL, mach);
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
