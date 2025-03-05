//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/r61524.h>
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

mqR61524 *mq_r61524_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void write_PRDR(mqR61524 *R61524, u32 value)
{
    R61524->PRDR = value;
}

static u32 read_r61524(mqMMIO *io, u32 addr, int size)
{
    mqMachine *mach = io->userdata;
    mqR61524 *R61524 = mach->modules[moduleID];
    if(!R61524)
        return 0;
    (void)addr;
    (void)size;

    mq_log(MQ_LOG_DEBUG, "read_r61524: read register %03x (TODO, returning 0)",
        R61524->selectedRegister);
    return 0;
}

static void write_r61524(mqMMIO *io, u32 addr, u32 value, int size)
{
    mqMachine *mach = io->userdata;
    mqR61524 *R61524 = mach->modules[moduleID];
    if(!R61524)
        return;
    (void)addr;

    if((R61524->PRDR & 0x10) == 0) {
        // mq_log(MQ_LOG_DEBUG, "write_r61524: selecting register %03x", value);
        R61524->selectedRegister = value;
        return;
    }

    mqDisplay *display = mach->display;

    switch(R61524->selectedRegister) {
    case 0x200: /* HADDR */
        R61524->HADDR = value;
        break;
    case 0x201: /* VADDR */
        R61524->VADDR = value;
        break;
    case 0x202: /* DATA */
        // TODO[r61524]: Honor write direction and window
        // Be careful that x avis is inverted
        // circuit10's Mario Kart would be a good test for that
        if(!display || display->format != MQ_DISPLAY_FORMAT_RGB565 ||
           display->width != 396 || display->height != 224)
            mq_log(MQ_LOG_ERROR, "r61524: invalid display!");
        u16 *data = display->data + 2 * (396 * R61524->VADDR + R61524->HADDR);
        if(size == 2) {
            data[0] = value;
            R61524->HADDR++;
        }
        if(size == 4) {
            data[0] = value >> 16;
            data[1] = value;
            R61524->HADDR += 2;
        }
        if(size == 1)
            mq_log(MQ_LOG_ERROR, "r61524: ignoring write of %d bytes!", size);
        if(R61524->HADDR >= 396) {
            R61524->HADDR -= 396;
            R61524->VADDR++;
            if(R61524->VADDR == 224) {
                // Full Frame: set dirty only now?
                mq_log(MQ_LOG_DEBUG, "r61524: Finished full frame");
                R61524->VADDR = 0;
            }
        }
        display->dirty = true;
        break;
    case 0x210:
        R61524->HSA = value;
        break;
    case 0x211:
        R61524->HEA = value;
        break;
    case 0x212:
        R61524->VSA = value;
        break;
    case 0x213:
        R61524->VEA = value;
        break;
    default:
        mq_log(MQ_LOG_DEBUG, "write_r61524: register %04x <- %08x",
            R61524->selectedRegister, value);
    }
}

bool mq_r61524_setup(mqMachine *mach)
{
    mqPage *pg94000 = mq_memory_getPage(mach->memory, 0x94000000);
    mqPage *pgb4000 = mq_memory_getPage(mach->memory, 0xb4000000);
    mqPage *pga4050 = mq_memory_getPage(mach->memory, 0xa4050000);
    if(!pg94000 || !pgb4000 || !pga4050)
        return false;

    mqR61524 *R61524 = calloc(1, sizeof *R61524);
    if(!R61524)
        return false;

    /* Set the initial window state to fullscreen */
    // TODO[r61524]: Set CASIO's settings as the default because add-ins?
    R61524->HSA = 0;
    R61524->HEA = 395;
    R61524->VSA = 0;
    R61524->VEA = 223;

    bool ok = true;
    // TODO[r61524]: PRDR should not be managed by the R61524 driver!
    ok &= mq_page_mapRegister8(pga4050, "PRDR", 0xa405013c,
        NULL, write_PRDR, &R61524->PRDR, R61524);

    int ioID = mq_page_addIO(pgb4000, "R61524", MQ_MMIO_UNSIZED, read_r61524,
        write_r61524, NULL, mach);
    /* Map at least 32 bytes of the interface so we can use DMA */
    ok &= mq_page_mapIO(pgb4000, ioID, 0xb4000000, 32, 1);

    /* Repeat in the P1 page */
    ioID = mq_page_addIO(pg94000, "R61524", MQ_MMIO_UNSIZED, read_r61524,
        write_r61524, NULL, mach);
    ok &= mq_page_mapIO(pg94000, ioID, 0x94000000, 32, 1);

    if(ok)
        mach->modules[moduleID] = R61524;
    else
        free(R61524);
    return ok;
}

static void mq_r61524_cleanup(mqMachine *mach)
{
    mqR61524 *R61524 = mach->modules[moduleID];
    if(R61524)
        free(R61524);
}
MQ_HOOK_REGISTER(module_cleanup, mq_r61524_cleanup)
