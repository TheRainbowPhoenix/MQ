//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // TODO: Remove

// Note: The CRD object management paradigm is as follows:
// 1. There are three functions: create, reset, destroy.
//    - create allocates memory and sets a default, consistent state.
//    - reset releases any resources and sets the default state.
//    - destroy releases any resources and frees the object.
// 2. The workflow is: create reset* destroy.
// 3. Objects are *always* in a consistent state.
// 4. reset sets the default state without leaking resources no matter what the
//    previous state is. In particular it leaves no dangling pointers.
// 5. High-level code might introduce "init" functions that behave like reset
//    but set a new state different from the initial state.

static mqChunk *mq_chunk_create(void);
static void mq_chunk_reset(mqChunk *chunk);
static void mq_chunk_destroy(mqChunk *chunk);

static mqMMIOPage *mq_MMIOPage_create(void);
static void mq_MMIOPage_reset(mqMMIOPage *chunk);
static void mq_MMIOPage_destroy(mqMMIOPage *chunk);

//=== Object management functions ============================================//

mqMemory *mq_memory_create(void)
{
    mqMemory *mem = malloc(sizeof *mem);
    if(!mem)
        return NULL;

    for(int i = 0; i < 0x1000; i++)
        mem->chunks[i] = MQ_CHUNKPTR_NULL;
    return mem;
}

void mq_memory_reset(mqMemory *mem)
{
    for(int i = 0; i < 0x1000; i++) {
        mqChunkPointer ch = mem->chunks[i];
        mem->chunks[i] = MQ_CHUNKPTR_NULL;

        if(MQ_CHUNKPTR_ISBUFFER(ch))
            free(ch);
        else if(!MQ_CHUNKPTR_ISNULL(ch))
            mq_chunk_destroy(MQ_CHUNKPTR_DETAILS(ch));
    }
}

void mq_memory_destroy(mqMemory *mem)
{
    mq_memory_reset(mem);
    free(mem);
}

static mqChunk *mq_chunk_create(void)
{
    mqChunk *chunk = malloc(sizeof *chunk);
    if(!chunk)
        return NULL;

    for(int i = 0; i < 256; i++)
        chunk->pages[i] = MQ_PAGEPTR_NULL;
    return chunk;
}

static void mq_chunk_reset(mqChunk *chunk)
{
    for(int i = 0; i < 256; i++) {
        mqPagePointer pg = chunk->pages[i];
        chunk->pages[i] = MQ_PAGEPTR_NULL;

        if(MQ_PAGEPTR_ISBUFFER(pg))
            free(pg);
        else if(!MQ_PAGEPTR_ISNULL(pg))
            mq_MMIOPage_destroy(MQ_PAGEPTR_MMIOPAGE(pg));
    }
}

static void mq_chunk_destroy(mqChunk *chunk)
{
    mq_chunk_reset(chunk);
    free(chunk);
}

static mqMMIOPage *mq_MMIOPage_create(void)
{
    mqMMIOPage *mmpg = calloc(1, sizeof *mmpg);
    /* All fields have default value 0. */
    return mmpg;
}

void mq_MMIOPage_reset(mqMMIOPage *mmpg)
{
    free(mmpg->map);
    free(mmpg->io);
    memset(mmpg, 0, sizeof *mmpg);
}

void mq_MMIOPage_destroy(mqMMIOPage *mmpg)
{
    mq_MMIOPage_reset(mmpg);
    free(mmpg);
}

//=== Memory configuration functions =========================================//

bool mq_memory_createBufferChunk(mqMemory *mem, u32 addr, void *buffer)
{
    u32 chunkNum = addr >> 20;
    if(!MQ_CHUNKPTR_ISNULL(mem->chunks[chunkNum]))
        return false;

    /* Automatically allocate a 1-MB buffer if one is not supplied. */
    if(!buffer) {
        buffer = calloc(1, 1 << 20);
        if(!buffer)
            return false;
    }

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKBUFFER(buffer);
    return true;
}

mqChunk *mq_memory_createChunk(mqMemory *mem, u32 addr)
{
    u32 chunkNum = addr >> 20;
    if(!MQ_CHUNKPTR_ISNULL(mem->chunks[chunkNum]))
        return NULL;

    mqChunk *chunk = mq_chunk_create();
    if(!chunk)
        return NULL;

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKDETAILS(chunk);
    return chunk;
}

bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer)
{
    u32 pageNum = (addr & 0xfffff) >> 8;
    if(!MQ_PAGEPTR_ISNULL(chunk->pages[pageNum]))
        return false;

    /* Automatically allocate a 4-kB buffer if one is not supplied. */
    if(!buffer) {
        buffer = calloc(1, 1 << 12);
        if(!buffer)
            return false;
    }

    chunk->pages[pageNum] = MQ_PAGEPTR_MKBUFFER(buffer);
    return true;
}

//=== Large-scale memory access functions ====================================//

bool mq_memory_load(mqMemory *mem, u32 baseAddr, void const *data, int size)
{
    // TODO: mq_memory_load: This requires MASSIVE optimizations
    for(int i = 0; i < size; i++) {
        u8 value = ((u8 *)data)[i];

        u32 addr = baseAddr + i;
        mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
        addr &= 0xfffff;

        if(MQ_CHUNKPTR_ISNULL(chunkPtr))
            return false;
        if(MQ_CHUNKPTR_ISBUFFER(chunkPtr)) {
            mq_buffer_write8(MQ_CHUNKPTR_BUFFER(chunkPtr), addr, value);
            continue;
        }

        mqChunk *chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);
        mqPagePointer pagePtr = chunk->pages[addr >> 8];
        addr &= 0xfff;

        if(MQ_PAGEPTR_ISNULL(pagePtr))
            return false;
        if(MQ_PAGEPTR_ISBUFFER(pagePtr)) {
            mq_buffer_write8(MQ_PAGEPTR_BUFFER(pagePtr), addr, value);
            continue;
        }

        /* We don't accept MMIO write in this function. */
        return false;
    }

    return true;
}

//=== Standard memory access functions =======================================//

static bool _mq_chunk_read_pure(
    mqChunk const *chunk, u32 addr, int size, u32 *out)
{
    if(chunk) {
        u32 chunkOffset = addr & 0xfffff;
        mqPagePointer pagePtr = chunk->pages[chunkOffset >> 12];

        if(MQ_LIKELY(MQ_PAGEPTR_ISBUFFER(pagePtr))) {
            void *page = MQ_PAGEPTR_BUFFER(pagePtr);
            if(size == 4)
                *out = mq_buffer_read32(page, chunkOffset & 0xfff);
            else if(size == 2)
                *out = mq_buffer_read16(page, chunkOffset & 0xfff);
            else
                *out = mq_buffer_read8(page, chunkOffset & 0xfff);
            return true;
        }

        else if(MQ_LIKELY(!MQ_PAGEPTR_ISNULL(pagePtr))) {
            // TODO: MMIO reads
            fprintf(stderr, "TODO: MMIO page read!\n");
            exit(1);
        }
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
    mqPagePointer pagePtr = chunk->pages[chunkOffset >> 12];

    if(MQ_LIKELY(MQ_PAGEPTR_ISBUFFER(pagePtr))) {
        void *page = MQ_PAGEPTR_BUFFER(pagePtr);
        if(size == 4)
            mq_buffer_write32(page, chunkOffset & 0xfff, value);
        else if(size == 2)
            mq_buffer_write16(page, chunkOffset & 0xfff, value);
        else
            mq_buffer_write8(page, chunkOffset & 0xfff, value);
        return true;
    }

    else if(MQ_LIKELY(!MQ_PAGEPTR_ISNULL(pagePtr))) {
        // TODO: MMIO writes
        fprintf(stderr, "TODO: MMIO page write!\n");
        exit(1);
    }

    return false;
}

bool mq_memory_write_pure(mqMemory *mem, u32 addr, int size, u32 value)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return false;

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
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

//=== Miscellaneous ==========================================================//

struct mqMemory_Stats mq_memory_stats(mqMemory const *mem)
{
    struct mqMemory_Stats s = { 0 };
    if(!mem)
        return s;

    for(uint i = 0; i < 0x1000; i++) {
        if(MQ_CHUNKPTR_ISNULL(mem->chunks[i]))
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
