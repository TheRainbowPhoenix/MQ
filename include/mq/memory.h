//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/100 ---------------------------------------------------------------//
// mq.memory: A configurable address space structure
//
// This header defines mqMemory, the type of a configurable address space that
// can map addresses to more or less any emulation behavior. This includes:
//
// - Standard byte-addressed memory backed by a buffer in the emulator;
// - Extra complications for standard memory such as repeating within a larger
//   region or translating addresses through MMU;
// - Memory-mapped I/O and peripheral modules.
//
// The memory is organized in a two-level hierarchy to allow sufficiently fine-
// grained control of memory contents without having overly large arrays. The
// first level is divided in 1 MiB "chunks" while the second level is divided
// in 4 kiB "pages". The large swaths of memory-mapped cache/TLB addressing
// space are handled with special handlers.
//
// For the calculator models where performance is most critical (fx-CG and
// fx-CP) both RAM and ROM are aligned on chunk boundaries, which allows an
// optimization where entire chunks are designated as buffer-backed memory,
// removing the need to allocate and dereference a page array.
//
// In terms of memory usage, a typical memory map is expected to use in the
// order of 10000 pointers, with a fixed 4096 chunk pointers and a comparable
// amount for filling in the page arrays (25-50 chunks). Page arrays are needed
// for every chunk that has MMIO or small memory areas.
//
// The storage method for buffers is 32-bit big-endian, i.e., the emulated
// memory is divided in 4-byte units whose big-endian interpretation is stored
// in host memory as host-endian (which is little endian in most cases). This
// avoids endianness conversion for the most common access size. For 8-bit and
// 16-bit accesses the address is manipulated with a xor to get back to the
// expected memory layout. Usually I'd use explicit endianness conversion since
// that's a cheap instruction, but we have an Emscripten target and WebAssembly
// still doesn't have an endian-swap instruction.
//---

#ifndef MQ_MEMORY_H
#define MQ_MEMORY_H

#include <mq/cpu.h>
MQ_START_DEFS

/* ILRAM is 16kB and repeats for 2MB.
   XYRAM are 8kB each, repeat for 64kB and the entire block repeats for 4MB. */

/* Pointer approach.
   High-level unit is 1M for entire 4G address space: 4096 pointers.
   Flag indicates if trivial or not -> trivial for ROM, RAM.
   Otherwise go down to 4k units: 256 pointers.
   -> 2M for ILRAM
   -> 4M for XYRAM
   -> ? for RSRAM
   -> Registers: a40, a41, a44, a46, a47, a4d
   -> Registers: fc1, fd0, fe0, fe2, fe3, fec, ff0, ff2, ff8
   -> TLB: f20..f71 and more? (aaah)
   -> Cache: f00..f10 and f40..f50? (aaah)
   -> SPU2 RAM: fe2, fe3

   In theory, 0x1c000000..0x1fffffff mirrors 0xfc000000..0xffffffff.

   Total: 4096 + 256 * ≈20   ≈ 8000 pointers. Nice split! */

// TODO[memory]: Option to keep MMU disabled

/* A chunk pointer whose least significant bit is stolen to indicate whether
   the entire chunk is a buffer (0) or not (1). */
typedef void *mqChunkPointer;
/* Macros for punning around the chunk pointers. */
#define MQ_CHUNKPTR_NULL ((mqChunkPointer)1)
#define MQ_CHUNKPTR_ISNULL(PTR) ((uintptr_t)(PTR) == 1)
#define MQ_CHUNKPTR_ISBUFFER(PTR) (((uintptr_t)(PTR) & 1) == 0)
#define MQ_CHUNKPTR_ISDETAILS(PTR) (((uintptr_t)(PTR) & 1) != 0)
#define MQ_CHUNKPTR_BUFFER(PTR) ((void *)(PTR))
#define MQ_CHUNKPTR_DETAILS(PTR) ((mqChunk *)((uintptr_t)(PTR) & -2))
#define MQ_CHUNKPTR_MKBUFFER(PTR) ((mqChunkPointer)(PTR))
#define MQ_CHUNKPTR_MKDETAILS(PTR) ((mqChunkPointer)((uintptr_t)(PTR) | 1))

/* A buffer of emulated memory. */
struct mqMemoryBuffer {
    char const *name;
    void *data;
    u32 size;
};

struct mqMemory {
    /* Array of pointers to chunk info.
       * If the LSB is clear this is a direct pointer a buffer representation,
         and the pointer cannot be NULL.
       * If the LSB is set this is a pointer to an mqChunk structure after
         clearing said bit if the result is not NULL.
       * An empty entry is indicated with (mqChunkPointer)1. */
    // TODO: Use the other bit for writeability.
    mqChunkPointer chunks[0x1000];

    // TODO: Cache, MMU, mapping description, lazy mappings...
    struct mqMemoryBuffer *buffers;
    int bufferCount;
};

/* A page pointer whose two least significant bits are stolen to indicate
   whether the page is a buffer or MMIO, and the MMIO map size. */
typedef void *mqPagePointer;
/* Macros for punning around the page pointers. */
#define MQ_PAGEPTR_NULL ((mqPagePointer)1)
#define MQ_PAGEPTR_ISNULL(PTR) ((uintptr_t)(PTR) == 1)
#define MQ_PAGEPTR_ISBUFFER(PTR) (((uintptr_t)(PTR) & 1) == 0)
#define MQ_PAGEPTR_ISMMIOPAGE(PTR) (((uintptr_t)(PTR) & 1) != 0)
#define MQ_PAGEPTR_BUFFER(PTR) ((void *)(PTR))
#define MQ_PAGEPTR_MMIOPAGE(PTR) ((mqMMIOPage *)((uintptr_t)(PTR) & -2))
#define MQ_PAGEPTR_MKBUFFER(PTR) ((mqPagePointer)(PTR))
#define MQ_PAGEPTR_MKMMIOPAGE(PTR) ((mqPagePointer)((uintptr_t)(PTR) | 1))

/* Chunk contents for a chunk that's not optimized to be a whole buffer. The
   chunk is divided into 4-kB pages, all of which can independently be backed
   by a buffer or define an MMIO range. */
struct mqChunk {
   /* Array of pointers to pages.
      * If the LSB is clear this is a buffer pointer, and it can't be NULL.
      * If the LSB is set this clearing it yields an mqMMIOPage * if not NULL.
      * An empty page is indicated with (mqPagePointer)1. */
    // TODO: Use the other bit for writeability.
   mqPagePointer pages[256];
};

/* An MMIO range of up to 4 kiB. This structure defines fairly rich info
   structures and independently maps address within the range to these
   structures. This is to avoid repeating 8-byte pointers over a large area. */
struct mqMMIOPage {
    /* Length of the range. The range always starts at offset 0 in the page. */
    int length;
    /* Mapping of bytes [0..length) in the page to IO structures. Value 0 is
       the absence of a mapping. Value v ≥ 1 maps to io[v - 1]. */
    u8 *map;
    /* List of IO structures, indexed by map[address & 0xfff]. */
    struct mqMMIO *io;
    int ioCount;
    /* io is preallocated; number of entries used in the array. */
    int ioUsed;
};

/* Flags setting general behaviors for I/O accesses. */
enum {
    /* Access size options--these are coded on low bits so that access with
       size S is allowed if (S & flags != 0). Not critical, but neat. */
    MQ_MMIO_SIZE_1      = 0x0001,
    MQ_MMIO_SIZE_2      = 0x0002,
    MQ_MMIO_SIZE_4      = 0x0004,
    MQ_MMIO_UNSIZED     = 0x0007,

    /* Default read behaviors where the value of an u8/u16/u32 is returned
       directly without invoking a read callback. In this case the read
       function is ignored. The `value` pointer is an u8/u16/u32 *. */
    MQ_MMIO_READU8      = 0x0010,
    MQ_MMIO_READU16     = 0x0020,
    MQ_MMIO_READU32     = 0x0040,

    /* The behavior of read/write functions does not depent on access address
       or size. This can be set if the address/alignment constraints are
       completely captured by the map in mqMMIOPage and alignment flags. This
       affects the prototypes of read/write, see struct mqMMIO. */
    MQ_MMIO_RELOC       = 0x0100,
};

/* Information on a memory-mapped I/O unit. */
struct mqMMIO {
    /* MQ_MMIO_* flags */
    int flags;
    /* Read and write function pointers. The prototypes are
       - u32 read(void *data, u32 addr, int size)
         void write(void *data, u32 value, u32 addr, int size)
         if MQ_MMIO_RELOC is clear;
       - u32 read(void *data)
         void write(void *data, u32 value)
         if MQ_MMIO_RELOC is set.
       If MQ_MMIO_READU*, the read pointer is ignored and can be NULL. NULL
       pointers in any other situation is treated as a fatal error. */
    void *read;
    void *write;
    /* Value pointer for MQ_MMIO_READ{U8,U16,u32} */
    void *value;
    /* Data pointer passed as first argument to read/write */
    void *data;
    /* Printable name; no specific constraints */
    char const *name;
};

typedef struct mqMemory mqMemory;
typedef struct mqChunk mqChunk;
typedef struct mqMMIOPage mqMMIOPage;
typedef struct mqMMIO mqMMIO;

//=== Memory configuration ===================================================//

/* CRD functions for mqMemory. The default state is an empty memory with no
   mapped chunks. */
mqMemory *mq_memory_create(void);
void mq_memory_reset(mqMemory *mem);
void mq_memory_destroy(mqMemory *mem);

/* Allocate a new zero-initialized buffer owned by the mqMemory. The buffer can
   be mapped freely to memory, including partially, multiple times, and in
   overlapping fashions. The name string can be used to later query the buffer
   in mq_memory_getBuffer(), unless it's NULL, then the buffer has no name. The
   name string is not copied. */
void *mq_memory_allocBuffer(mqMemory *mem, char const *name, u32 size);

/* Get a previously allocated buffer by name, NULL if nonexistant. If size is
   not NULL, it receives the size of the buffer. */
void *mq_memory_getBuffer(mqMemory *mem, char const *name, u32 *size);

/* Create a buffer chunk; the backing data cannot be NULL. Returns true on
   success, false if the chunk already exists (in which case the call is a
   no-op). */
bool mq_memory_createBufferChunk(mqMemory *mem, u32 addr, void *data);

/* Create a standard (broken-down) chunk. Returns a pointer to the chunk
   structure, NULL if the chunk already exists. */
mqChunk *mq_memory_createChunk(mqMemory *mem, u32 addr);

/* Create a buffer page in a chunk, the backing data cannot be NULL. Returns
   true on success, false if the page already exists (in which case the call is
   a no-op). */
bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer);

/* Create an MMIO page in a chunk. The given map length and MMIO count are used
   to preallocate if not zero. Returns true on success, false if the page
   already exists (in which case the call is a no-op). */
mqMMIOPage *mq_chunk_createMMIOPage(
    mqChunk *chunk, u32 addr, int preallocLength, int preallocIOCount);

/* Add an IO unit to an MMIO page. This doesn't map the IO to memory yet.
   Returns an integer identifying the IO to be used in mq_page_mapIO(), or a
   negative value on error. */
int mq_page_addIO(mqMMIOPage *mmpg, char const *name, int flags, void *read,
    void *write, void *value, void *data);

/* Map an IO unit previously added to the given page. The high bits of addr
   are ignored. */
bool mq_page_mapIO(mqMMIOPage *mmpg, int ioID, u32 address, int size);

/* Add and map a peripheral register (common kind of IO). Registers are
   accessed from a single address with their size as the alignment. flags are
   implied to be `MQ_MMIO_SIZE_<SIZE> | MQ_MMIO_RELOC`, with an extra
   `MQ_MMIO_READU<SIZE>` if the read function is set to NULL.  */
bool mq_page_mapRegister8(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u8 *value, void *data);
bool mq_page_mapRegister16(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u16 *value, void *data);
bool mq_page_mapRegister32(mqMMIOPage *mmpg, char const *name, u32 addr,
   void *read, void *write, u32 *value, void *data);

/* Create a series of chunks or pages matching the given memory interval. The
   start address must be page-aligned; the size will be rounded up to the next
   page-size multiple. This function creates buffer chunks or buffer pages
   as needed to cover the interval, which needs to be initially empty. If
   `buffer` is NULL, one will be allocated (contiguously). On error, returns
   false; the memory will be partially modified.

   WARNING: MQ assumes that blocks do not touch. Specifically, it assumes that
            contiguous pieces of emulated-data (like strings of arrays) are not
            split over two blocks and hence belong to a single buffer. If you
            need to map two blocks directly one after another, make sure they
            are backed by two consecutive regions of a single large buffer. */
bool mq_memory_createBlock(mqMemory *mem, u32 addr, u32 size, void *buffer);

/* Load data from a host buffer into memory. This applies endianness swaps to
   match the internal buffer format and works across chunk and page boundaries.
   Returns true on success, false if the designated range is not entirely
   covered by buffer chunks and buffer pages. */
bool mq_memory_load(mqMemory *mem, u32 addr, void const *data, int size);

//=== Memory access functions ================================================//

/* Inlined functions for accessing raw buffers. */

MQ_INLINE u8 mq_buffer_read8(void const *buffer, u32 offset)
{
   return *((u8 *)buffer + (offset ^ 3));
}
MQ_INLINE u16 mq_buffer_read16(void const *buffer, u32 offset)
{
   return *(u16 *)((u8 *)buffer + (offset ^ 2));
}
MQ_INLINE u32 mq_buffer_read32(void const *buffer, u32 offset)
{
   return *(u32 *)((u8 *)buffer + offset);
}

MQ_INLINE void mq_buffer_write8(void const *buffer, u32 offset, u8 value)
{
   *((u8 *)buffer + (offset ^ 3)) = value;
}
MQ_INLINE void mq_buffer_write16(void const *buffer, u32 offset, u16 value)
{
   *(u16 *)((u8 *)buffer + (offset ^ 2)) = value;
}
MQ_INLINE void mq_buffer_write32(void const *buffer, u32 offset, u32 value)
{
   *(u32 *)((u8 *)buffer + offset) = value;
}

/* Main memory access functions. */

// internal
bool _mq_chunk_read(
    mqCpu *cpu, mqChunk const *chunk, u32 addr, int size, u32 *out);
bool _mq_chunk_read_pure(
    mqChunk const *chunk, u32 addr, int size, u32 *out, mqMMIOPage **mmpg);

/* Read 32 bits from memory at the given address. On success, returns true and
   sets *out. On error, raises an exception with the machine, leaves *out
   unchanged, and returns false. The fast path is inlined while the slow paths
   are handled in the internal `_mq_chunk_read()` function. The output pointer
   should disappear with inlining and the alignment check can be contextually
   optimized out. */
MQ_INLINE bool mq_memory_read32(mqCpu *cpu, mqMemory *mem, u32 addr, u32 *out)
{
    // TODO: Memory access exception type: instruction read vs. data read.
    if(MQ_UNLIKELY(addr & 3))
        return mq_cpu_raiseException_false(cpu, SH_EXC_READ_ADDR, addr);

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        *out = mq_buffer_read32(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);
        return true;
    }

    return _mq_chunk_read(cpu, MQ_CHUNKPTR_DETAILS(chunkPtr), addr, 4, out);
}

/* Read 16 bits from the given address. */
MQ_INLINE bool mq_memory_read16(mqCpu *cpu, mqMemory *mem, u32 addr, u32 *out)
{
    // TODO: Memory access exception type: instruction read vs. data read.
    if(MQ_UNLIKELY(addr & 1))
        return mq_cpu_raiseException_false(cpu, SH_EXC_READ_ADDR, addr);

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        *out = mq_buffer_read16(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);
        return true;
    }

    return _mq_chunk_read(cpu, MQ_CHUNKPTR_DETAILS(chunkPtr), addr, 2, out);
}

/* Read 8 bits from the given address. */
MQ_INLINE bool mq_memory_read8(mqCpu *cpu, mqMemory *mem, u32 addr, u32 *out)
{
    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        *out = mq_buffer_read8(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);
        return true;
    }

    return _mq_chunk_read(cpu, MQ_CHUNKPTR_DETAILS(chunkPtr), addr, 1, out);
}

/* Read an opcode from the given address. The is a pure read. Returns 0 in case
   of error, which is an invalid opcode anyway. */
MQ_INLINE u32 mq_memory_read_opcode(mqCpu *cpu, mqMemory *mem, u32 addr)
{
    if(MQ_UNLIKELY(addr & 1)) {
        mq_cpu_raiseException_false(cpu, SH_EXC_INS_ADDR, addr);
        return 0;
    }

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr)))
        return mq_buffer_read16(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);

    mqChunk *chunk = MQ_CHUNKPTR_DETAILS(chunkPtr);
    u32 out;
    bool b = _mq_chunk_read_pure(chunk, addr, 2, &out, NULL);
    return b ? out : 0;
}

/* Write to memory. Returns true on success, false if an exception occurs. */
bool mq_memory_write(mqCpu *cpu, mqMemory *mem, u32 addr, int size, u32 value);

/* Read/write from memory, with no exceptions/side-effects. Just returns the
   status and value. This is used for UI code that manipulates the memory. */
bool mq_memory_read_pure(mqMemory *mem, u32 addr, int size, u32 *out);
bool mq_memory_write_pure(mqMemory *mem, u32 addr, int size, u32 value);

/* Get direct access to emulated memory via a pointer. This function returns a
   pointer into an emulated buffer, pointing to at least as many bytes as the
   emulated pointer points to in the emulated program. Note that the internal
   buffer is 4-byte endian-swapped. If this function is not called with a
   4-aligned parameter then data might need to be fetched before the teturn
   pointer of this function. */
void *mq_memory_access(mqMemory *mem, u32 addr);

//=== Misc. information ======================================================//

struct mqMemory_Stats {
    /* Number of mapped chunks backed by buffers, details; total. */
    uint bufferChunks;
    uint detailedChunks;
    uint totalChunks;
    /* Number of chunks with no conventional memory at all. */
    uint pureMMIOChunks;

    /* Number of pages mapped to buffers in chunk details. */
    uint bufferPages;
};

struct mqMemory_Stats mq_memory_stats(mqMemory const *mem);

MQ_END_DEFS
#endif /* MQ_MEMORY_H */
