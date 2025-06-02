//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/pfc.h>
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

mqPFC *mq_pfc_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void write_PORTM(mqPFC *PFC, u8 value)
{
    PFC->PORTM = value;
}

static void write_PORTB_CTRL(mqPFC *PFC, u16 value)
{
    PFC->PORTB_CTRL = value;
}

static void write_PORTM_CTRL(mqPFC *PFC, u16 value)
{
    PFC->PORTM_CTRL = value;
}

static u32 read_PFC(mqMMIO *io, u32 addr, int size) 
{
    mqMachine *mach = io->userdata;
    mqPFC *PFC = mach->modules[moduleID];
    if (!PFC)
        return 0xffffffff;
    (void)addr;
    (void)size;

    // mq_log(MQ_LOG_DEBUG, "read_PFC @ 0x%08x (size: %d)", addr, size);

    uint row = 0;

    if (PFC->PORTB_CTRL != 0xAAAA) { 
        // Rows 0-7
        u16 smask = PFC->PORTB_CTRL ^ 0xAAAA;
        while (smask >> (row*2) > 0b11) row++;
    }
    else { 
        // Rows 8-9
        row = 8 + ((PFC->PORTM_CTRL & 0b11) >> 1);
    }

    if (row > 9) {
        mq_log(MQ_LOG_WARNING, "Request for keyboard row %d which does not exist!\n", row);
        return 0xffffffff;
    }
    
    int start, count, shift;
    if (row < 5) {
        start = 5*6 + (4 - row) * 5 - 1;
        count = 5;
        shift = 2;
    }
    else {
        start = (9 - row) * 6 - 1;
        count = 6;
        shift = 1;
    }

    int status = 0;
    for (int i = 0; i < count; i++) {
        uint key_id = start + count - i;

        if (key_id >= mach->keyboard->keyCount) {
            mq_log(MQ_LOG_WARNING, "Request for key %d which does not exist!\n", key_id);
            continue;
        }

        if (mach->keyboard->keyStatus[key_id]) {
            status |= 1 << (i + shift);
        }
    }

    // Return selected row status
    return ~status;
}

bool mq_pfc_setup(mqMachine *mach)
{
    mqPage *pg = mq_memory_getPage(mach->memory, 0xa4000102);
    if(!pg)
        return false;

    mqPFC *PFC = calloc(1, sizeof *PFC);
    if(!PFC)
        return false;

    bool ok = true;
    
    int ioID = mq_page_addIO(pg, "KB_PORTA", MQ_MMIO_UNSIZED, read_PFC,
        NULL, NULL, mach);
    ok &= mq_page_mapIO(pg, ioID, KB_PORTA, 1, 1);

    ok &= mq_page_mapRegister8(pg, "KB_PORTM", KB_PORTM,
        NULL, write_PORTM, &PFC->PORTM, PFC);
    ok &= mq_page_mapRegister16(pg, "KB_PORTB_CTRL", KB_PORTB_CTRL,
        NULL, write_PORTB_CTRL, &PFC->PORTB_CTRL, PFC);
    ok &= mq_page_mapRegister16(pg, "KB_PORTM_CTRL", KB_PORTM_CTRL,
        NULL, write_PORTM_CTRL, &PFC->PORTM_CTRL, PFC);

    if(ok)
        mach->modules[moduleID] = PFC;
    else
        free(PFC);
    return ok;
}

static void mq_pfc_cleanup(mqMachine *mach)
{
    mqPFC *PFC = mach->modules[moduleID];
    if(PFC)
        free(PFC);
}
MQ_HOOK_REGISTER(module_cleanup, mq_pfc_cleanup)
