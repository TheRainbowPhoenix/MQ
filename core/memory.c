//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/memory.h>
#include <mq/machine.h>
#include <mq/hooks.h>
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

    mem->buffers = NULL;
    mem->bufferCount = 0;
    return mem;
}

void mq_memory_reset(mqMemory *mem)
{
    for(int i = 0; i < 0x1000; i++) {
        mqChunkPointer ch = mem->chunks[i];
        mem->chunks[i] = MQ_CHUNKPTR_NULL;

        if(ch != MQ_CHUNKPTR_NULL && MQ_CHUNKPTR_ISDETAILS(ch))
            mq_chunk_destroy(MQ_CHUNKPTR_DETAILS(ch));
    }

    for(int i = 0; i < mem->bufferCount; i++)
        free(mem->buffers[i].data);
    free(mem->buffers);

    mem->buffers = NULL;
    mem->bufferCount = 0;
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

        if(pg != MQ_PAGEPTR_NULL && MQ_PAGEPTR_ISMMIOPAGE(pg))
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

static bool mq_MMIOPage_allocMap(mqMMIOPage *mmpg, int length)
{
    if(length <= mmpg->length)
        return true;

    u8 *new_map = realloc(mmpg->map, length * sizeof *mmpg->map);
    if(!new_map)
        return false;

    memset(new_map + mmpg->length, 0x00,
        (length - mmpg->length) * sizeof *mmpg->map);
    mmpg->length = length;
    mmpg->map = new_map;
    return true;
}

static bool mq_MMIOPage_allocIO(mqMMIOPage *mmpg, int ioCount)
{
    if(ioCount <= mmpg->ioCount)
        return true;

    mqMMIO *new_io = realloc(mmpg->io, ioCount * sizeof *mmpg->io);
    if(!new_io)
        return false;

    mmpg->io = new_io;
    mmpg->ioCount = ioCount;
    return true;
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

void *mq_memory_allocBuffer(mqMemory *mem, char const *name, u32 size)
{
    int newSize = (mem->bufferCount + 1) * sizeof *mem->buffers;
    struct mqMemoryBuffer *newBuffers = realloc(mem->buffers, newSize);
    if(!newBuffers)
        return NULL;

    void *data = calloc(1, size);
    if(!data) {
        free(newBuffers);
        return NULL;
    }

    struct mqMemoryBuffer *b = &newBuffers[mem->bufferCount];
    mem->buffers = newBuffers;
    mem->bufferCount++;

    b->name = name;
    b->data = data;
    b->size = size;
    return data;
}

void *mq_memory_getBuffer(mqMemory *mem, char const *name, u32 *size)
{
    if(!name)
        return NULL;

    for(int i = 0; i < mem->bufferCount; i++) {
        struct mqMemoryBuffer *b = &mem->buffers[i];
        if(b->name && !strcmp(b->name, name)) {
            if(size)
                *size = b->size;
            return b->data;
        }
    }

    return NULL;
}

bool mq_memory_createBufferChunk(mqMemory *mem, u32 addr, void *buffer)
{
    u32 chunkNum = addr >> 20;
    if(!buffer || !MQ_CHUNKPTR_ISNULL(mem->chunks[chunkNum]))
        return false;

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKBUFFER(buffer);
    return true;
}

mqChunk *mq_memory_getOrCreateChunk(mqMemory *mem, u32 addr)
{
    u32 chunkNum = addr >> 20;
    mqChunkPointer ptr = mem->chunks[chunkNum];

    if(!MQ_CHUNKPTR_ISNULL(ptr)) {
        if(MQ_CHUNKPTR_ISDETAILS(ptr))
            return MQ_CHUNKPTR_DETAILS(ptr);
        else
            return NULL;
    }

    mqChunk *chunk = mq_chunk_create();
    if(!chunk)
        return NULL;

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKDETAILS(chunk);
    return chunk;
}

bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer)
{
    u32 pageNum = (addr & 0xfffff) >> 12;
    if(!buffer || !MQ_PAGEPTR_ISNULL(chunk->pages[pageNum]))
        return false;

    chunk->pages[pageNum] = MQ_PAGEPTR_MKBUFFER(buffer);
    return true;
}

mqMMIOPage *mq_chunk_getOrCreateMMIOPage(
    mqChunk *chunk, u32 addr, int length, int ioCount)
{
    u32 pageNum = (addr & 0xfffff) >> 12;
    mqPagePointer ptr = chunk->pages[pageNum];

    if(!MQ_PAGEPTR_ISNULL(ptr)) {
        if(MQ_PAGEPTR_ISMMIOPAGE(ptr))
            return MQ_PAGEPTR_MMIOPAGE(ptr);
        else
            return NULL;
    }

    mqMMIOPage *mmpg = mq_MMIOPage_create();
    if(!mmpg)
        return NULL;
    if(!mq_MMIOPage_allocMap(mmpg, length)
        || !mq_MMIOPage_allocIO(mmpg, ioCount)) {
        mq_MMIOPage_destroy(mmpg);
        return NULL;
    }

    chunk->pages[pageNum] = MQ_PAGEPTR_MKMMIOPAGE(mmpg);
    return mmpg;
}

int mq_page_addIO(mqMMIOPage *mmpg, char const *name, int flags, void *read,
    void *write, void *value, void *data)
{
    /* Reallocate the IO array if we need more space. */
    if(mmpg->ioUsed >= 256)
        return -1;
    if(mmpg->ioUsed >= mmpg->ioCount
        && !mq_MMIOPage_allocIO(mmpg, mmpg->ioUsed + 4))
        return -1;

    int index = mmpg->ioUsed;
    mqMMIO *io = &mmpg->io[index];
    io->name = name;
    io->flags = flags;
    io->read = read;
    io->write = write;
    io->value = value;
    io->data = data;
    mmpg->ioUsed++;
    return index + 1;
}

bool mq_page_mapIO(mqMMIOPage *mmpg, int ioID, u32 address, int size)
{
    address &= 0xfff;
    if(address + size > 0x1000)
        size = 0x1000 - address;
    if((uint)ioID > 0xff || size < 0
        || !mq_MMIOPage_allocMap(mmpg, address + size))
        return false;

    for(int i = 0; i < size; i++)
        mmpg->map[address + i] = ioID;
    return true;
}

bool mq_page_mapRegister8(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u8 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_1 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU8;
    int ioID = mq_page_addIO(mmpg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(mmpg, ioID, addr, 1);
}

bool mq_page_mapRegister16(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u16 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_2 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU16;
    int ioID = mq_page_addIO(mmpg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(mmpg, ioID, addr, 1);
}

bool mq_page_mapRegister32(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u32 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_4 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU32;
    int ioID = mq_page_addIO(mmpg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(mmpg, ioID, addr, 1);
}

bool mq_memory_createBlock(mqMemory *mem, u32 addr, u32 size, void *buffer)
{
    if(!buffer)
        return NULL;

    /* Mind the fact that addr + size might be 2³² which we can't compute
       directly. */
    if((addr & 0xfff) != 0 || !size || (size - 1 > ~addr))
        return false;
    if(size & 0xfff)
        size = (size | 0xfff) + 1;

    /* Allocate chunks or pages in sequence. */
    while(size > 0) {
        /* Try to allocate a chunk if we're on a chunk boundary, needs at least
           1 MB more memory, and the chunk doesn't already exist in a broken-
           down form. */
        if(!(addr & 0xfffff) && size >= 0x100000 &&
                mq_memory_createBufferChunk(mem, addr, buffer)) {
            size -= 0x100000;
            addr += 0x100000;
            buffer += 0x100000;
            continue;
        }

        /* Otherwise, fall back to allocating a page. */
        mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
        if(!mq_memory_getOrCreateChunk(mem, addr))
            return false;
        else if(MQ_CHUNKPTR_ISBUFFER(chunkPtr))
            return false;

        chunkPtr = mem->chunks[addr >> 20];
        mqChunk *chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);
        if(!mq_chunk_createBufferPage(chunk, addr, buffer))
            return false;

        size -= 0x1000;
        addr += 0x1000;
        buffer += 0x1000;
    }

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

bool _mq_chunk_read_pure(
    mqChunk const *chunk, u32 addr, int size, u32 *out, mqMMIOPage **mmpg)
{
    if(!chunk)
        return false;

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

    if(mmpg && MQ_LIKELY(!MQ_PAGEPTR_ISNULL(pagePtr)))
        *mmpg = MQ_PAGEPTR_MMIOPAGE(pagePtr);

    return false;
}

bool _mq_chunk_read(
    mqMachine *mach, mqChunk const *chunk, u32 addr, int size, u32 *out)
{
    mqMMIOPage *mmpg = NULL;
    if(_mq_chunk_read_pure(chunk, addr, size, out, &mmpg))
        return true;

    u32 pgAddr = addr & 0xfff;
    int ioID;
    if(mmpg && mmpg->length > (int)pgAddr && (ioID = mmpg->map[pgAddr])) {
        mqMMIO *io = &mmpg->io[ioID - 1];

        /* Check access size */
        if(size & io->flags) {
            /* Handle default reads; vaguely in frequency order */
            if(io->flags & MQ_MMIO_READU32)
                return *(u32 *)io->value;
            if(io->flags & MQ_MMIO_READU16)
                return *(u16 *)io->value;
            if(io->flags & MQ_MMIO_READU8)
                return *(u8 *)io->value;

            /* Use the generic functions */
            if(MQ_UNLIKELY(!io->read))
                mq_log(MQ_LOG_ERROR, "NULL MMIO at %08x (r)", addr);
            else if(MQ_LIKELY(io->flags & MQ_MMIO_RELOC)) {
                u32 (*f)(void *data) = io->read;
                *out = f(io->data);
                return true;
            }
            else {
                u32 (*f)(void *data, u32 addr, int size) = io->read;
                *out = f(io->data, addr, size);
                return true;
            }
        }
    }

    if(mq_callhook_memory_read(mach, mach->memory, addr, size, out))
        return true;

    // TODO: Handle special cases related to TLB/Cache areas

    /* In add-in modes we have a 4-kB page mapped at NULL. This has the effect
       of masking enough programming errors (even in well-written programs)
       that we *need* to emulate it. I don't want to set up a page for it as
       the memory shouldn't exist, hopefully none of the bugged code relies on
       reading bootcode bytes from there! */
    // TODO: Limit NULL page accesses to add-in mode
    if(addr < 0x00001000) {
        mq_log(MQ_LOG_WARNING, "[PC=%08x] NULL page read @ %08x -> return 0",
            mach->cpu.pc, addr);
        *out = 0;
        return true;
    }
    /* Memory accesses outside bounds of defined memory raise TLB errors when
       accessing U0/P0 but just silently return undefined values in P1-P4. */
    else if(addr >= 0x80000000) {
        mq_log(MQ_LOG_WARNING,
            "[PC=%08x] unhandled read @ %08x -> returning 0",
            mach->cpu.pc, addr);
        *out = 0;
        return true;
    }

    return mq_cpu_raiseException_false(&mach->cpu, SH_EXC_READ_ADDR, addr);
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

    // TODO: Hook writes

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

bool mq_memory_write(
    mqMachine *mach, mqMemory *mem, u32 addr, int size, u32 value)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return mq_cpu_raiseException_false(&mach->cpu, SH_EXC_WRITE_ADDR, addr);

    if(mq_memory_write_pure(mem, addr, size, value))
        return true;

    /* Writes to NULL pages; see equivalent for reads. */
    // TODO: Limit NULL writes to add-ins
    if(addr < 0x00001000) {
        mq_log(MQ_LOG_WARNING,
            "[PC=%08x] NULL page write @ %08x -> ignoring",
            mach->cpu.pc, addr);
        return true;
    }
    /* Memory writes outside bounds of defined memory raise TLB errors when
       accessing U0/P0 but just silently do nothing in P1-P4. */
    // TODO: Memory access exception type: instruction read vs. data read.
    if(addr < 0x80000000)
        return mq_cpu_raiseException_false(&mach->cpu, SH_EXC_READ_ADDR, addr);

    return true;
}

bool mq_memory_read_pure(mqMemory *mem, u32 addr, int size, u32 *out)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return false;

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        if(size == 4)
            *out = mq_buffer_read32(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        else if(size == 2)
            *out = mq_buffer_read16(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        else
            *out = mq_buffer_read8(MQ_CHUNKPTR_BUFFER(chunkPtr), chunkOff);
        return true;
    }

    mqChunk *ch = MQ_CHUNKPTR_DETAILS(chunkPtr);
    return _mq_chunk_read_pure(ch, addr, size, out, NULL);
}

void *mq_memory_access(mqMemory *mem, u32 addr)
{
    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOffset = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr)))
        return MQ_CHUNKPTR_BUFFER(chunkPtr) + chunkOffset;

    mqChunk *ch = MQ_CHUNKPTR_DETAILS(chunkPtr);
    if(!ch)
        return NULL;

    mqPagePointer pagePtr = ch->pages[chunkOffset >> 12];
    if(MQ_LIKELY(MQ_PAGEPTR_ISBUFFER(pagePtr)))
        return MQ_PAGEPTR_BUFFER(pagePtr) + (chunkOffset & 0xfff);

    return NULL;
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
            pages += (ch->pages[j] != MQ_PAGEPTR_NULL);

        s.bufferPages += pages;
        s.pureMMIOChunks += (pages == 0);
    }

    return s;
}
