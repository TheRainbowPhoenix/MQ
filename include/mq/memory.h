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
// space are handled with a special case.
//
// For the calculator models where performance is most critical (fx-CG and
// fx-CP) both RAM and ROM are aligned on chunk boundaries, which allows an
// optimization where entire chunks are designated as buffer-backed memory,
// removing the need to allocate and dereference a page array. This fast path,
// which takes a dozen or so instructions, is inlined.
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
/* Macros for punning around the chunk pointer's. */
#define MQ_CHUNKPTR_ISBUFFER(PTR) (((uintptr_t)(PTR) & 1) == 0)
#define MQ_CHUNKPTR_BUFFER(PTR) ((void *)(PTR))
#define MQ_CHUNKPTR_DETAILS(PTR) ((mqChunk *)((uintptr_t)(PTR) & -2))
#define MQ_CHUNKPTR_MKBUFFER(PTR) ((mqChunkPointer)(PTR))
#define MQ_CHUNKPTR_MKDETAILS(PTR) ((mqChunkPointer)((uintptr_t)(PTR) | 1))

struct mqMemory {
    /* Array of pointers to chunk info. If the LSB is clear this is a direct
       pointer a buffer representation (cannot be NULL). If the LSB is set this
       is a pointer to an mqChunkDetails structure after clearing said bit. An
       empty entry is thus indicated with value (void *)1. */
    mqChunkPointer chunks[0x1000];
};

/* Read-write functions for a chunk. */
typedef bool mq_chunk_read_t(u32 addr, int size, u32 *result);
typedef bool mq_chunk_write_t(u32 addr, int size, u32 value);

/* Chunk contents for a chink that's not optimized to be a whole buffer. */
struct mqChunk {
   /* Array of pointers to raw pages. If the pointer is not NULL then the page
      is a raw buffer access, otherwise accesses to the page are handled by the
      read/write functions below. */
   void *pages[256];

   /* General read/write functions for pages that don't use buffers. addresses
      are given as full 32-bit values because this is generally used for MMIO,
      for which the full address is more recognizable. */
   mq_chunk_read_t *read;
   mq_chunk_write_t *write;
};

typedef struct mqMemory mqMemory;
typedef struct mqChunk mqChunk;

//=== Memory configuration ===================================================//

mqMemory *mq_memory_alloc(void);
void mq_memory_free(mqMemory *mem);

/* Reset to an empty memory, freeing all existing chunks and pages. */
void mq_memory_init(mqMemory *mem);

/* Create a buffer chunk. If `buffer` is NULL, allocates one, initialized with
   zero. Returns true on success, false if the chunk already exists. */
bool mq_memory_createBufferChunk(mqMemory *mem, u32 addr, void *buffer);

/* Create a standard chunk. Returns a pointer to the chunk structure, NULL if
   the chunk already exists. */
mqChunk *mq_memory_createChunk(
    mqMemory *mem, u32 addr, mq_chunk_read_t *read, mq_chunk_write_t *write);

/* Create a buffer page in a chunk. If `buffer` is NULL, allocates one. The
   high-order bits of the address are ignored. Returns true on success, false
   if the page already exists. */
bool mq_chunk_createBufferPage(mqChunk *chunk, u32 addr, void *buffer);

/* Load data from a buffer into memory. This applies endianness swaps to match
   the internal buffer format and works across chunk and page boundaries.
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

/* Read 32 bits from memory at the given address. On success, returns true and
   sets *out. On error, raises an exception with the machine and returns false.
   The fast path is inlined while the slow paths are handled in the internal
   `_mq_chunk_read()` function. The output pointer should disappear with
   inlining and the alignment check can be contextually optimized out. */
MQ_INLINE bool mq_memory_read32(mqCpu *cpu, mqMemory *mem, u32 addr, u32 *out)
{
    // TODO: Memory access exception type: instruction read vs. data read.
    if(MQ_UNLIKELY(addr & 3))
        return mq_cpu_raiseException_false(cpu, SH_EXC_READ_ADDR, addr);

    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(chunkPtr && MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
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
    if(chunkPtr && MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        *out = mq_buffer_read16(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);
        return true;
    }

    return _mq_chunk_read(cpu, MQ_CHUNKPTR_DETAILS(chunkPtr), addr, 2, out);
}

/* Read 8 bits from the given address. */
MQ_INLINE bool mq_memory_read8(mqCpu *cpu, mqMemory *mem, u32 addr, u32 *out)
{
    mqChunkPointer chunkPtr = mem->chunks[addr >> 20];
    if(chunkPtr && MQ_LIKELY(MQ_CHUNKPTR_ISBUFFER(chunkPtr))) {
        *out = mq_buffer_read8(MQ_CHUNKPTR_BUFFER(chunkPtr), addr & 0xfffff);
        return true;
    }

    return _mq_chunk_read(cpu, MQ_CHUNKPTR_DETAILS(chunkPtr), addr, 1, out);
}

/* Write to memory. Returns true on success, false if an exception occurs. */
bool mq_memory_write(mqCpu *cpu, mqMemory *mem, u32 addr, int size, u32 value);

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
