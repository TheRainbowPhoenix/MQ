//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/modules/cmod.h>
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

mqCmod *mq_cmod_get(mqMachine *mach)
{
    return mach->modules ? mach->modules[moduleID] : NULL;
}

static void write_RTSTRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x30) >> 5];
    (void)size;
    RT->RTSTR = value & 0x01;
    // TODO[cmod]: Consequences of writing to RTSTR
    if(RT->RTSTR)
        mq_log(MQ_LOG_ERROR, "not starting RTC timer!");
}

#include <stdio.h>
static void write_RTCORn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x34) >> 5];
    (void)size;
    RT->RTCOR = value;
    // TODO[cmod]: Consequences of setting RTCOR < RTCNT?
}

static u32 read_RTCNTn(struct mqMMIO *io, u32 addr, int size)
{
    // TODO[cmod]: read_RTCNT: tick down upon reading?
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x38) >> 5];
    (void)size;
    return RT->RTCNT;
}

static void write_RTCNTn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x38) >> 5];
    (void)size;
    RT->RTCNT = value;
    // TODO[cmod]: Consequences of setting RTCNT--immediate interrupt?
}

static void write_RTCRn(struct mqMMIO *io, u32 addr, u32 value, int size)
{
    mqCmod *Cmod = io->userdata;
    mqCmod_RTCTimer *RT = &Cmod->timers[((addr & 0xfff) - 0x3c) >> 5];
    (void)size;
    RT->RTCR = (RT->RTCR & value & 0x02) | (value & 0x01);
    // TODO[cmod]: Consequences of writing to RTCR
}

bool mq_cmod_setup(mqMachine *mach)
{
    mqMemory *mem = mach->memory;
    mqPage *pg44d = mq_memory_getPagePrealloc(mem, 0xa44d0000, 0xdc, 24);
    if(!pg44d)
        return false;

    mqCmod *Cmod = calloc(1, sizeof *Cmod);
    if(!Cmod)
        return false;

    /* Initialize timer constants and timers to a value where they don't
       automatically send out an interrupt. Not sure what the default value
       should be, but this certainly inspires more confidence. */
    for(int i = 0; i < 6; i++) {
        Cmod->timers[i].RTCOR = 0xffffffff;
        Cmod->timers[i].RTCNT = 0xffffffff;
    }

    bool ok = true;
    u32 RT_addresses[6] = {
        0xa44d0030, 0xa44d0050, 0xa44d0070, 0xa44d0090,
        0xa44d00b0, 0xa44d00d0,
    };

    for(int i = 0; i < 6; i++) {
        u32 a = RT_addresses[i];
        mqCmod_RTCTimer *RT = &Cmod->timers[i];
        int ioID;

        ioID = mq_page_addIO(pg44d, "RTSTRn", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
            NULL, write_RTSTRn, &RT->RTSTR, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCORn", MQ_MMIO_SIZE_4 | MQ_MMIO_READU32,
            NULL, write_RTCORn, &RT->RTCOR, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a+4, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCNTn", MQ_MMIO_SIZE_4, read_RTCNTn,
            write_RTCNTn, NULL, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a+8, 1, 0);

        ioID = mq_page_addIO(pg44d, "RTCRn", MQ_MMIO_SIZE_1 | MQ_MMIO_READU8,
            NULL, write_RTCRn, &RT->RTCR, Cmod);
        ok &= mq_page_mapIO(pg44d, ioID, a+12, 1, 0);
    }

    if(ok)
        mach->modules[moduleID] = Cmod;
    else
        free(Cmod);
    return ok;
}

static void mq_cmod_cleanup(mqMachine *mach)
{
    mqCmod *Cmod = mach->modules[moduleID];
    if(Cmod)
        free(Cmod);
}
MQ_HOOK_REGISTER(module_cleanup, mq_cmod_cleanup)
