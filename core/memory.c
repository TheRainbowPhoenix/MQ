//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/memory.h>

u32 _mq_chunk_read(mqCpu *cpu, mqChunk const *chunk, u32 addr, int size)
{
    if(chunk) {
        u32 chunkOffset = addr & 0xfffff;

        void *pagePtr = chunk->pages[chunkOffset >> 12];
        if(MQ_LIKELY(pagePtr != NULL)) {
            if(size == 4)
                return mq_buffer_read32(pagePtr, chunkOffset & 0xfff);
            else if(size == 2)
                return mq_buffer_read16(pagePtr, chunkOffset & 0xfff);
            else
                return mq_buffer_read32(pagePtr, chunkOffset & 0xfff);
        }

        if(MQ_LIKELY(chunk->read != NULL)) {
            u32 result;
            if(MQ_LIKELY(chunk->read(addr, 4, &result)))
                return result;
        }
    }

    // TODO: Handle special cases related to TLB/Cache areas

    /* Memory accesses outside bounds of defined memory raise TLB errors when
       accessing U0/P0 but just silently return undefined values in P1-P4. */
    if(addr < 0x80000000)
        // TODO: Memory access exception type: instruction read vs. data read.
        mq_cpu_raiseException(cpu, SH_EXC_READ_ADDR, addr);
    return 0;
}
