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

static mqPage *mq_page_create(void);
static void mq_page_reset(mqPage *chunk);
static void mq_page_destroy(mqPage *chunk);

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
        mqChunkPointer ptr = mem->chunks[i];
        mem->chunks[i] = MQ_CHUNKPTR_NULL;

        mqChunk *ch = MQ_CHUNKPTR_GET(ptr);
        if(ch)
            mq_chunk_destroy(ch);
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
        mqPagePointer ptr = chunk->pages[i];
        chunk->pages[i] = MQ_PAGEPTR_NULL;

        mqPage *pg = MQ_PAGEPTR_GET(ptr);
        if(pg)
            mq_page_destroy(pg);
    }
}

static void mq_chunk_destroy(mqChunk *chunk)
{
    mq_chunk_reset(chunk);
    free(chunk);
}

static mqPage *mq_page_create(void)
{
    mqPage *pg = calloc(1, sizeof *pg);
    /* All fields have default value 0. */
    return pg;
}

static bool mq_page_allocMap(mqPage *pg, int length)
{
    if(length <= pg->length)
        return true;

    u8 *new_map = realloc(pg->map, length * sizeof *pg->map);
    if(!new_map)
        return false;

    memset(new_map + pg->length, 0, (length - pg->length) * sizeof *pg->map);
    pg->length = length;
    pg->map = new_map;
    return true;
}

static bool mq_page_allocIO(mqPage *pg, int ioCount)
{
    if(ioCount <= pg->ioCount)
        return true;

    mqMMIO *new_io = realloc(pg->io, ioCount * sizeof *pg->io);
    if(!new_io)
        return false;

    pg->io = new_io;
    pg->ioCount = ioCount;
    return true;
}

void mq_page_reset(mqPage *pg)
{
    free(pg->map);
    free(pg->io);
    memset(pg, 0, sizeof *pg);
}

void mq_page_destroy(mqPage *pg)
{
    mq_page_reset(pg);
    free(pg);
}

//=== Configuration of memory buffers ========================================//

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
    if(!buffer || mem->chunks[chunkNum] != MQ_CHUNKPTR_NULL)
        return false;

    mem->chunks[chunkNum] = MQ_CHUNKPTR_MKBUFFER(buffer);
    return true;
}

bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer)
{
    u32 pageNum = (addr & 0xfffff) >> 12;
    if(!buffer || chunk->pages[pageNum] != MQ_PAGEPTR_NULL)
        return false;

    chunk->pages[pageNum] = MQ_PAGEPTR_MKBUFFER(buffer);
    return true;
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
        mqChunk *chunk = mq_memory_getChunk(mem, addr);
        if(!chunk || !mq_chunk_createBufferPage(chunk, addr, buffer))
            return false;

        size -= 0x1000;
        addr += 0x1000;
        buffer += 0x1000;
    }

    return true;
}

//=== Configuration of memory-mapped I/O =====================================//

mqChunk *mq_memory_getChunk(mqMemory *mem, u32 addr)
{
    u32 chunkNum = addr >> 20;
    mqChunkPointer ptr = mem->chunks[chunkNum];
    mqChunk *chunk = MQ_CHUNKPTR_GET(ptr);

    if(ptr == MQ_CHUNKPTR_NULL) {
        chunk = mq_chunk_create();
        mem->chunks[chunkNum] = MQ_CHUNKPTR_MK(chunk);
    }
    return chunk;
}

mqPage *mq_memory_getPage(mqMemory *mem, u32 addr)
{
    return mq_memory_getPagePrealloc(mem, addr, 0, 0);
}

mqPage *mq_memory_getPagePrealloc(
    mqMemory *mem, u32 addr, int length, int ioCount)
{
    mqChunk *chunk = mq_memory_getChunk(mem, addr);
    return
        chunk ? mq_chunk_getPagePrealloc(chunk, addr, length, ioCount) : NULL;
}

mqPage *mq_chunk_getPage(mqChunk *chunk, u32 addr)
{
    return mq_chunk_getPagePrealloc(chunk, addr, 0, 0);
}

mqPage *mq_chunk_getPagePrealloc(
    mqChunk *chunk, u32 addr, int length, int ioCount)
{
    u32 pageNum = (addr & 0xfffff) >> 12;
    mqPagePointer ptr = chunk->pages[pageNum];
    mqPage *pg = MQ_PAGEPTR_GET(ptr);

    if(ptr == MQ_PAGEPTR_NULL) {
        pg = mq_page_create();
        if(!pg)
            return NULL;
        if((length && !mq_page_allocMap(pg, length)) ||
           (ioCount && !mq_page_allocIO(pg, ioCount))) {
            mq_page_destroy(pg);
            return NULL;
        }
        chunk->pages[pageNum] = MQ_PAGEPTR_MK(pg);
    }
    return pg;
}

int mq_page_addIO(mqPage *pg, char const *name, int flags, void *read,
    void *write, void *value, void *userdata)
{
    /* Reallocate the IO array if we need more space. */
    if(pg->ioUsed >= 256)
        return -1;
    if(pg->ioUsed >= pg->ioCount
        && !mq_page_allocIO(pg, pg->ioUsed + 4))
        return -1;

    int index = pg->ioUsed;
    mqMMIO *io = &pg->io[index];
    io->name = name;
    io->flags = flags;
    io->read = read;
    io->write = write;
    io->value = value;
    io->userdata = userdata;
    pg->ioUsed++;
    return index + 1;
}

bool mq_page_mapIO(mqPage *pg, int ioID, u32 address, int size, int align)
{
    align -= (align != 0);

    address &= 0xfff;
    if(address + size > 0x1000)
        size = 0x1000 - address;
    if((uint)ioID > 0xff || size < 0 || !mq_page_allocMap(pg, address + size))
        return false;

    for(int i = 0; i < size; i++) {
        if(((address + i) & align) == 0)
            pg->map[address + i] = ioID;
    }
    return true;
}

bool mq_page_mapRegister8(mqPage *pg, char const *name, u32 addr,
   void *read, void *write, u8 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_1 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU8;
    int ioID = mq_page_addIO(pg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(pg, ioID, addr, 1, 1);
}

bool mq_page_mapRegister16(mqPage *pg, char const *name, u32 addr,
   void *read, void *write, u16 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_2 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU16;
    int ioID = mq_page_addIO(pg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(pg, ioID, addr, 1, 1);
}

bool mq_page_mapRegister32(mqPage *pg, char const *name, u32 addr,
   void *read, void *write, u32 *value, void *data)
{
    int flags = MQ_MMIO_SIZE_4 | MQ_MMIO_RELOC;
    if(!read)
        flags |= MQ_MMIO_READU32;
    int ioID = mq_page_addIO(pg, name, flags, read, write, value, data);
    return (ioID >= 0) && mq_page_mapIO(pg, ioID, addr, 1, 1);
}

/* Helper function for reading string I/Os. */
static u32 readStringIO(struct mqMMIO *io, u32 addr, int size)
{
    char const *str = io->value;
    uintptr_t userdata = (uintptr_t)io->userdata;
    u16 offset = (addr & 0xffff) - (userdata & 0xffff);
    u16 string_size = userdata >> 16;

    if(offset + size > string_size) {
        mq_log(MQ_LOG_WARNING,
            "readStringIO: %dB @%08x reads out-of-bounds of %08x/%d",
            size, addr, addr - offset, (int)string_size);
    }

    u32 res = 0;
    for(int i = 0; i < size; i++) {
        res <<= 8;
        if(offset + i < string_size)
            res += str[offset + i];
    }

    // mq_log(MQ_LOG_DEBUG,
    //     "readStringIO: %dB @%08x from %08x/%d \"%.*s\"+%d (%s) -> %08x",
    //     size, addr, addr - offset, (int)string_size,
    //     (int)string_size, str, offset, io->name, res);

    return res;
}

bool mq_page_mapString(
    mqPage *pg, char const *name, u32 addr, void *str, u16 size)
{
    /* Pack both the string size and its start address in the data field. */
    MQ_STATIC_ASSERT(sizeof(void *) >= 4);
    MQ_STATIC_ASSERT(sizeof(uintptr_t) >= 4);
    uintptr_t data = (size << 16) | (addr & 0xffff);

    int ioID = mq_page_addIO(pg, name, MQ_MMIO_UNSIZED, readStringIO, NULL,
        str, (void *)data);
    return (ioID >= 0) && mq_page_mapIO(pg, ioID, addr, size, 1);
}

//=== TODO: Unclassified memory functions ====================================//

void mq_memory_unbindArea(mqMemory *mem, u32 addr, u32 length)
{
    if((addr & 0xfffff) || (length & 0xfffff))
        return;

    int startChunkNum = addr >> 20;
    int chunkCount = length >> 20;

    for(int i = startChunkNum; i < startChunkNum + chunkCount; i++) {
        mqChunkPointer chunkPtr = mem->chunks[i];
        mqChunk *chunk = MQ_CHUNKPTR_GET(chunkPtr);
        if(!chunk) {
            mem->chunks[i] = MQ_CHUNKPTR_NULL;
            continue;
        }

        int remainingPages = 0;

        for(int j = 0; j < 256; j++) {
            mqPagePointer pagePtr = chunk->pages[j];
            if(!MQ_PAGEPTR_GET(pagePtr)) {
                chunk->pages[j] = MQ_PAGEPTR_NULL;
                continue;
            }
            else remainingPages++;
        }

        if(remainingPages == 0) {
            mq_chunk_destroy(chunk);
            mem->chunks[i] = MQ_CHUNKPTR_NULL;
        }
        else {
            mq_log(MQ_LOG_WARNING, "%d pages left while unbinding chunk %08x",
                remainingPages, (u32)i << 20);
        }
    }
}

bool mq_memory_copyBuffersInChunk(
    mqMemory *mem, u32 sourceAddress, u32 targetAddress)
{
    if((sourceAddress & 0xfffff) || (targetAddress & 0xfffff))
        return false;

    int sourceChunkNum = (sourceAddress | 0x80000000) >> 20;
    int targetChunkNum = targetAddress >> 20;

    if(mem->chunks[targetChunkNum] != MQ_CHUNKPTR_NULL)
        return false;

    mqChunkPointer srcPtr = mem->chunks[sourceChunkNum];

    /* Copy null or buffer chunks directly */
    if(!MQ_CHUNKPTR_GET(srcPtr)) {
        mem->chunks[targetChunkNum] = srcPtr;
        return true;
    }

    /* Copy broken-down chunks with a new chunk allocation. We can't easily
       share the same mqChunkPointer because all entries of mem->chunks have
       contractually independent ownership so it'd be freed twice. */
    bool ok = true;
    for(int i = 0; i < 256; i++) {
        ok &= mq_memory_copyBuffersInPage(mem, sourceAddress, targetAddress);
        sourceAddress += (1 << 12);
        targetAddress += (1 << 12);
    }

    return ok;
}

bool mq_memory_copyBuffersInPage(
    mqMemory *mem, u32 sourceAddress, u32 targetAddress)
{
    if((sourceAddress & 0xfff) || (targetAddress & 0xfff))
        return false;

    int sourceChunkNum = (sourceAddress | 0x80000000) >> 20;
    if(mem->chunks[sourceChunkNum] == MQ_CHUNKPTR_NULL)
        return false;
    if(MQ_CHUNKPTR_ISBUFFER(mem->chunks[sourceChunkNum])) {
        // TODO[mq_memory_copyBuffersInPage]: Map page from buffer chunk
        mq_log(MQ_LOG_ERROR,
            "page copy to %08x from buffer chunk at %08x is TODO o(x_x)o",
            targetAddress, sourceAddress);
        return false;
    }
    mqChunk *sourceChunk = MQ_CHUNKPTR_GET(mem->chunks[sourceChunkNum]);

    /* Allocate the target chunk if needed. */
    mqChunk *targetChunk = mq_memory_getChunk(mem, targetAddress);
    if(!targetChunk)
        return false;

    int sourcePageNum = (sourceAddress & 0xfffff) >> 12;
    int targetPageNum = (targetAddress & 0xfffff) >> 12;
    mqPagePointer sourcePagePtr = sourceChunk->pages[sourcePageNum];

    if(MQ_PAGEPTR_GET(sourcePagePtr))
        return false;

    targetChunk->pages[targetPageNum] = sourcePagePtr;
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

        if(chunkPtr == MQ_CHUNKPTR_NULL)
            return false;
        if(MQ_CHUNKPTR_ISBUFFER(chunkPtr)) {
            mq_buffer_write8(MQ_CHUNKPTR_GETBUFFER(chunkPtr), addr, value);
            continue;
        }

        mqChunk *chunk = MQ_CHUNKPTR_GET(chunkPtr);
        mqPagePointer pagePtr = chunk->pages[addr >> 8];
        addr &= 0xfff;

        if(pagePtr == MQ_PAGEPTR_NULL)
            return false;
        if(MQ_PAGEPTR_ISBUFFER(pagePtr)) {
            mq_buffer_write8(MQ_PAGEPTR_GETBUFFER(pagePtr), addr, value);
            continue;
        }

        /* We don't accept MMIO write in this function. */
        return false;
    }

    return true;
}

//=== Standard memory access functions =======================================//

bool _mq_chunk_read_pure(
    mqChunk const *chunk, u32 addr, int size, u32 *out, mqPage **pg)
{
    if(!chunk)
        return false;

    u32 chunkOffset = addr & 0xfffff;
    mqPagePointer pagePtr = chunk->pages[chunkOffset >> 12];

    if(MQ_LIKELY(MQ_PAGEPTR_ISBUFFER(pagePtr))) {
        void *page = MQ_PAGEPTR_GETBUFFER(pagePtr);
        if(size == 4)
            *out = mq_buffer_read32(page, chunkOffset & 0xfff);
        else if(size == 2)
            *out = mq_buffer_read16(page, chunkOffset & 0xfff);
        else
            *out = mq_buffer_read8(page, chunkOffset & 0xfff);
        return true;
    }

    if(pg)
        *pg = MQ_PAGEPTR_GET(pagePtr);

    return false;
}

bool _mq_chunk_read(
    mqMachine *mach, mqChunk const *chunk, u32 addr, int size, u32 *out)
{
    mqPage *pg = NULL;
    if(_mq_chunk_read_pure(chunk, addr, size, out, &pg))
        return true;

    int ioID, pgAddr = addr & 0xfff;
    if(pg && pg->length > pgAddr && (ioID = pg->map[pgAddr])) {
        mqMMIO *io = &pg->io[ioID - 1];

        /* Check access size */
        if(size & io->flags) {
            /* Handle default reads; vaguely in frequency order */
            if(io->flags & MQ_MMIO_READU32) {
                *out = *(u32 *)io->value;
                return true;
            }
            if(io->flags & MQ_MMIO_READU16) {
                *out = *(u16 *)io->value;
                return true;
            }
            if(io->flags & MQ_MMIO_READU8) {
                *out = *(u8 *)io->value;
                return true;
            }

            /* Use the generic functions */
            if(MQ_UNLIKELY(!io->read))
                mq_log(MQ_LOG_ERROR, "NULL MMIO at %08x (r)", addr);
            else if(MQ_LIKELY(io->flags & MQ_MMIO_RELOC))
                *out = io->read_reloc(io->userdata);
            else
                *out = io->read(io, addr, size);
            return true;
        }
    }

    /* Let hooks handle special cases related to TLB/Cache areas */
    if(mq_callhook_memory_read(mach, mach->memory, addr, size, out))
        return true;

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
        void *page = MQ_PAGEPTR_GETBUFFER(pagePtr);
        if(size == 4)
            mq_buffer_write32(page, chunkOffset & 0xfff, value);
        else if(size == 2)
            mq_buffer_write16(page, chunkOffset & 0xfff, value);
        else
            mq_buffer_write8(page, chunkOffset & 0xfff, value);
        return true;
    }

    mqPage *pg = MQ_PAGEPTR_GET(pagePtr);
    int ioID, pgAddr = addr & 0xfff;
    if(pg && pg->length > pgAddr && (ioID = pg->map[pgAddr])) {
        mqMMIO *io = &pg->io[ioID - 1];

        /* Check access size */
        if(size & io->flags) {
            if(MQ_UNLIKELY(!io->write))
                mq_log(MQ_LOG_WARNING, "Write to ro I/O at %08x (r)", addr);
            else if(MQ_LIKELY(io->flags & MQ_MMIO_RELOC))
                io->write_reloc(io->userdata, value);
            else
                io->write(io, addr, value, size);
            return true;
        }
    }

    // TODO: Writes should set the dirty bit on MMU regions
    // To do that reasonably fast we need to associate buffers with relevant
    // MMU entries.

    // TODO: Hook writes

    return false;
}

bool mq_memory_write_pure(mqMemory *mem, u32 addr, int size, u32 value)
{
    if(MQ_UNLIKELY(addr & (size - 1)))
        return false;

    mqChunkPointer ptr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(ptr))) {
        if(size == 4)
            mq_buffer_write32(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff, value);
        else if(size == 2)
            mq_buffer_write16(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff, value);
        else
            mq_buffer_write8(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff, value);
        return true;
    }

    return _mq_chunk_write(MQ_CHUNKPTR_GET(ptr), addr, size, value);
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

    mqChunkPointer ptr = mem->chunks[addr >> 20];
    u32 chunkOff = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(ptr))) {
        if(size == 4)
            *out = mq_buffer_read32(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff);
        else if(size == 2)
            *out = mq_buffer_read16(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff);
        else
            *out = mq_buffer_read8(MQ_CHUNKPTR_GETBUFFER(ptr), chunkOff);
        return true;
    }

    return _mq_chunk_read_pure(MQ_CHUNKPTR_GET(ptr), addr, size, out, NULL);
}

void *mq_memory_access(mqMemory *mem, u32 addr)
{
    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    u32 chunkOffset = addr & 0xfffff;
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr)))
        return MQ_CHUNKPTR_GETBUFFER(chunkPtr) + chunkOffset;

    mqChunk *ch = MQ_CHUNKPTR_GET(chunkPtr);
    if(!ch)
        return NULL;

    mqPagePointer pagePtr = ch->pages[chunkOffset >> 12];
    if(MQ_LIKELY(MQ_PAGEPTR_ISBUFFER(pagePtr)))
        return MQ_PAGEPTR_GETBUFFER(pagePtr) + (chunkOffset & 0xfff);

    return NULL;
}

//=== Miscellaneous ==========================================================//

struct mqMemory_Stats mq_memory_stats(mqMemory const *mem)
{
    struct mqMemory_Stats s = { 0 };
    if(!mem)
        return s;

    for(uint i = 0; i < 0x1000; i++) {
        if(mem->chunks[i] == MQ_CHUNKPTR_NULL)
            continue;

        s.totalChunks++;
        if(MQ_CHUNKPTR_ISBUFFER(mem->chunks[i])) {
            s.bufferChunks++;
            continue;
        }

        mqChunk *ch = MQ_CHUNKPTR_GET(mem->chunks[i]);
        s.detailedChunks++;

        int pages = 0;
        for(int j = 0; j < 256; j++)
            pages += (ch->pages[j] != MQ_PAGEPTR_NULL);

        s.bufferPages += pages;
        s.pureMMIOChunks += (pages == 0);
    }

    return s;
}
