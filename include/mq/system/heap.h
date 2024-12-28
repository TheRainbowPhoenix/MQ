//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.system.heap: Heap syscall emulation
//
// I have no idea how CASIOWIN's heap is implemented, so I'm just going to use
// gint's allocator to emulate. I do know that the OS heap is much more lenient
// towards buffer overflows however (with a lot of buffer overflows flaring up
// in gint add-ins when gint's allocator became the default) so I arbitrarily
// add 8 extra bytes at the end of each allocation to compensate.
//
// FIXME: MQ heap is global instead of being attached to a machine.
//---

#ifndef MQ_SYSTEM_HEAP_H
#define MQ_SYSTEM_HEAP_H

#include <mq/defs.h>
MQ_START_DEFS

/* Heap statistics; sizes are in bytes. */
typedef struct {
    /* Free space, used space, peak used space over time */
    uint32_t free_memory;
    uint32_t used_memory;
    uint32_t peak_used_memory;
    /* Number of mallocs that failed because of not enough free space */
    int exhaustion_failures;
    /* Number of mallocs that failed because of fragmentation */
    int fragmentation_failures;
    /* Number of reallocs that successfully re-used the same location */
    int expanding_reallocs;
    /* Number of reallocs that moved the data around */
    int relocating_reallocs;

    /* Number of live blocks and maximum that metric reached over time. */
    int live_blocks;
    int peak_live_blocks;
    /* Total volume of allocs, number of allocs, failed allocs. */
    int total_volume;
    int total_blocks;
    int total_failures;

} mq_heap_stats_t;

/* Init the heap to use the designated range of emulated memory backed by the
   given buffer, which must be of size at least end - start. */
bool mq_heap_init(u32 start, u32 end, void *buffer);
/* Reset the heap. */
void mq_heap_reset(void);

/* Check if the heap was initialized. If it is, return the range. */
bool mq_heap_isInitialized(u32 *start, u32 *end);

/* Standard functions for using the allocated heap. These directly modify the
   backing buffer and return addresses as start-based u32. */
u32 mq_heap_malloc(size_t size);
void mq_heap_free(u32 ptr);
u32 mq_heap_realloc(u32 ptr, size_t size);

/* Get internal heap statistics. */
mq_heap_stats_t *mq_heap_stats(void);

typedef struct {
    /* Block sequence covers entire range */
    bool sequence_covers;
    /* Terminator block is correctly identified */
    bool sequence_terminator;
    /* Boundary tags are coherent with used tags */
    bool sequence_coherent_used;
    /* Footer sizes are correct in all free blocks */
    bool sequence_footer_size;
    /* All consecutive free blocks are merged */
    bool sequence_merged_free;
    /* The doubly-linked list structure is coherent */
    bool list_structure;
    /* The segregated lists cover all free blocks in the sequence */
    bool index_covers;
    /* The segregated lists contain blocks of correct sizes */
    bool index_class_separation;

} mq_heap_debug_t;

/* Get debug information from the heap. Undefined return value if the heap is
   not initialized. */
mq_heap_debug_t mq_heap_debuginfo(void);

MQ_END_DEFS
#endif /* MQ_SYSTEM_HEAP_H */
