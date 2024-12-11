//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/memory.h>
#include <stdlib.h>

mqMemory *mq_memory_alloc(void)
{
    mqMemory *mem = calloc(1, sizeof *mem);
    return mem;
}

void mq_memory_free(mqMemory *mem)
{
    free(mem);
}

static void _mq_chunk_free(mqChunk *chunk)
{
    for(int i = 0; i < 256; i++) {
        if(chunk->pages[i])
            free(chunk->pages[i]);
    }
}

void mq_memory_init(mqMemory *mem)
{
    for(int i = 0; i < 0x1000; i++) {
        mqChunkPointer ch = mem->chunks[i];
        mem->chunks[i] = NULL;

        if(ch && MQ_CHUNKPTR_ISBUFFER(ch))
            free(MQ_CHUNKPTR_BUFFER(ch));
        else if(ch)
            _mq_chunk_free(MQ_CHUNKPTR_DETAILS(ch));
    }
}

bool mq_memory_createBufferChunk(mqMemory *mem, u32 addr, void *buffer)
{
    u32 chunkNum = addr >> 20;
    if(mem->chunks[chunkNum])
        return false;

    if(!buffer) {
        buffer = calloc(1, 1 << 20);
        if(!buffer)
            return false;
    }

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKBUFFER(buffer);
    return true;
}

mqChunk *mq_memory_createChunk(
    mqMemory *mem, u32 addr, mq_chunk_read_t *read, mq_chunk_write_t *write)
{
    u32 chunkNum = addr >> 20;
    if(mem->chunks[chunkNum])
        return NULL;

    mqChunk *chunk = calloc(1, sizeof *chunk);
    if(!chunk)
        return NULL;

    chunk->read = read;
    chunk->write = write;
    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKDETAILS(chunk);
    return chunk;
}

bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer)
{
    u32 pageNum = (addr & 0xfffff) >> 8;
    if(chunk->pages[pageNum])
        return false;

    if(!buffer) {
        buffer = calloc(1, 1 << 12);
        if(!buffer)
            return false;
    }

    chunk->pages[pageNum] = buffer;
    return true;
}

bool mq_memory_load(mqMemory *mem, u32 baseAddr, void const *data, int size)
{
    // TODO: mq_memory_load: This requires MASSIVE optimizations
    for(int i = 0; i < size; i++) {
        u8 value = ((u8 *)data)[i];

        u32 addr = baseAddr + i;
        mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
        addr &= 0xfffff;

        if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
            mq_buffer_write8(MQ_CHUNKPTR_BUFFER(chunkPtr), addr, value);
            continue;
        }

        mqChunk *chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);
        void *page = chunk->pages[addr >> 8];
        addr &= 0xfff;

        if(MQ_LIKELY(page != NULL)) {
            mq_buffer_write8(page, addr, value);
            continue;
        }

        return false;
    }

    return true;
}

static bool _mq_chunk_read_pure(
    mqChunk const *chunk, u32 addr, int size, u32 *out)
{
    if(chunk) {
        u32 chunkOffset = addr & 0xfffff;

        void *pagePtr = chunk->pages[chunkOffset >> 12];
        if(MQ_LIKELY(pagePtr != NULL)) {
            if(size == 4)
                *out = mq_buffer_read32(pagePtr, chunkOffset & 0xfff);
            else if(size == 2)
                *out = mq_buffer_read16(pagePtr, chunkOffset & 0xfff);
            else
                *out = mq_buffer_read8(pagePtr, chunkOffset & 0xfff);
            return true;
        }

        if(MQ_LIKELY(chunk->read != NULL))
            return chunk->read(addr, size, out);
    }

    // TODO: Handle special cases related to TLB/Cache areas
    return false;
}

bool _mq_chunk_read(
    mqCpu *cpu, mqChunk const *chunk, u32 addr, int size, u32 *out)
{
    if(_mq_chunk_read_pure(chunk, addr, size, out))
        return true;

    /* Memory accesses outside bounds of defined memory raise TLB errors when
       accessing U0/P0 but just silently return undefined values in P1-P4. */
    // TODO: Memory access exception type: instruction read vs. data read.
    if(addr < 0x80000000)
        return mq_cpu_raiseException_false(cpu, SH_EXC_READ_ADDR, addr);

    *out = 0xffffffff;
    return true;
}

static bool _mq_chunk_write(
    mqChunk const *chunk, u32 addr, int size, u32 value)
{
    if(!chunk)
        return false;

    u32 chunkOffset = addr & 0xfffff;
    void *pagePtr = chunk->pages[chunkOffset >> 12];
    if(MQ_LIKELY(pagePtr != NULL)) {
        if(size == 4)
            mq_buffer_write32(pagePtr, chunkOffset & 0xfff, value);
        else if(size == 2)
            mq_buffer_write16(pagePtr, chunkOffset & 0xfff, value);
        else
            mq_buffer_write8(pagePtr, chunkOffset & 0xfff, value);
        return true;
    }

    if(MQ_LIKELY(chunk->write != NULL))
        return chunk->write(addr, size, value);
    return false;
}

bool mq_memory_write_pure(mqMemory *mem, u32 addr, int size, u32 value)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return false;

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(chunkPtr && MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        if(size == 4)
            mq_buffer_write32(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff, value);
        else if(size == 2)
            mq_buffer_write16(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff, value);
        else
            mq_buffer_write8(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff, value);
        return true;
    }

    return _mq_chunk_write(MQ_CHUNKPTR_DETAILS(chunkPtr), addr, size, value);
}

bool mq_memory_write(mqCpu *cpu, mqMemory *mem, u32 addr, int size, u32 value)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return mq_cpu_raiseException_false(cpu, SH_EXC_WRITE_ADDR, addr);

    if(mq_memory_write_pure(mem, addr, size, value))
        return true;

    /* Memory writes outside bounds of defined memory raise TLB errors when
       accessing U0/P0 but just silently do nothing in P1-P4. */
    // TODO: Memory access exception type: instruction read vs. data read.
    if(addr < 0x80000000)
        return mq_cpu_raiseException_false(cpu, SH_EXC_READ_ADDR, addr);

    return true;
}

bool mq_memory_read_pure(mqMemory *mem, u32 addr, int size, u32 *out)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return false;

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(chunkPtr && MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        if(size == 4)
            *out = mq_buffer_read32(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        else if(size == 2)
            *out = mq_buffer_read16(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        else
            *out = mq_buffer_read8(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        return true;
    }

    return _mq_chunk_read_pure(MQ_CHUNKPTR_DETAILS(chunkPtr), addr, size, out);
}

struct mqMemory_Stats mq_memory_stats(mqMemory const *mem)
{
    struct mqMemory_Stats s = { 0 };
    if(!mem)
        return s;

    for(uint i = 0; i < 0x1000; i++) {
        if(!mem->chunks[i])
            continue;

        s.totalChunks++;
        if(MQ_CHUNKPTR_ISBUFFER(mem->chunks[i])) {
            s.bufferChunks++;
            continue;
        }

        mqChunk *ch = MQ_CHUNKPTR_DETAILS(mem->chunks[i]);
        s.detailedChunks++;

        int pages = 0;
        for(int j = 0; j < 256; j++)
            pages += (ch->pages[i] != NULL);

        s.bufferPages += pages;
        s.pureMMIOChunks += (pages == 0);
    }

    return s;
}
