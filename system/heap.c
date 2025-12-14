//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

/* The following is a modification of gint's heap allocator. It skips gint-
   exclusive features, exposes stats to the emulator, and distinguishes guest
   pointers (u32, blockptr_t) from host pointers (void *, block_t *), with a
   mapping given by the resolve() function. The block structure is host-endian
   but this is hidden behind the syscall API anyway, so it doesn't matter. */

#include <mq/system/heap.h>
#include <mq/memory.h>
#include <mq/defs.h>
#include <string.h>

static u32 mq_heapBase = 0;
static u32 mq_heapEnd = 0;
static void *mq_heapBuffer = NULL;

static void *resolve(u32 address)
{
    if(address == 0)
        return NULL;
    if(address >= mq_heapBase && address < mq_heapEnd + 1)
        return mq_heapBuffer + (address - mq_heapBase);

    mq_log(MQ_LOG_WARNING, "out-of-bounds heap pointer %08x (heap offset %d)",
        address, (i32)address - mq_heapBase);
    return NULL;
}

typedef volatile struct {
    u32 :5;
    /* Marks the last block of the sequence */
    u32 last: 1;
    /* Whether the block is used; in general this can be kept implicit, but
       it has to be specified for the last block */
    u32 used: 1;
    /* Boundary tag, vital to implement way #2 to merge adjacent blocks */
    u32 previous_used: 1;
    /* Block size in bytes. */
    u32 size: 24;
} block_t;

MQ_STATIC_ASSERT(sizeof(block_t) == 4);

typedef u32 blockptr_t;

typedef mq_heap_stats_t stats_t;

/* index_t: Data structure at the root of the heap, indexing linked lists */
typedef struct {
    /* Entry points of the free lists for each block size */
    blockptr_t classes[16];
    /* Pointer to statistics, if used */
    stats_t stats;
} index_t;

//---
// Block-level operations
//---

/* Get a pointer to the block's footer. OOB: 0. */
static u32 *block_footer(blockptr_t b)
{
    block_t *bp = resolve(b);
    if(!bp) return NULL;

    u32 footer_address = b + sizeof(block_t) + bp->size;
    void *footer = resolve(footer_address - 12);
    return footer ? footer + 12 : NULL;
}

/* Get a pointer to the previous block's footer. OOB: 0. */
static u32 *previous_block_footer(blockptr_t b)
{
    void *footer = resolve(b - 12);
    return footer ? footer + 12 : NULL;
}

/* Next block in the sequence. OOB: 0. */
static blockptr_t next_block(blockptr_t b)
{
    block_t *bp = resolve(b);
    if(!bp || bp->last) return 0;
    return b + sizeof(block_t) + bp->size;
}

/* Previous block in the sequence, if it's free. OOB: 0. */
static blockptr_t previous_block_if_free(blockptr_t b)
{
    block_t *bp = resolve(b);
    if(!bp || bp->previous_used) return 0;

    /* The footer of the previous block indicates its size */
    u32 *footer = previous_block_footer(b);
    if(!footer) return 0;
    u32 previous_size = (footer[-1] & 1) ? 8 : footer[-3];
    return b - previous_size - sizeof(block_t);
}

/* Splits a used or free-floating block into a first block with (size) bytes
   and a free second block with the rest. Returns the address of the second
   block. If the initial block is too small to split, returns 0. OOB: 0. */
static blockptr_t split(blockptr_t b, int size)
{
    block_t *bp = resolve(b);
    if(!bp) return 0;

    size_t extra_size = bp->size - size;
    if(extra_size < sizeof(block_t) + 8) return 0;

    blockptr_t second = b + sizeof(block_t) + size;
    block_t *secondp = resolve(second);
    if(!secondp) return 0;
    secondp->last = bp->last;
    secondp->used = false;
    secondp->previous_used = bp->used;
    secondp->size = extra_size - sizeof(block_t);

    blockptr_t third = next_block(second);
    block_t *thirdp = resolve(third);
    if(thirdp) thirdp->previous_used = secondp->used;

    bp->last = 0;
    bp->size = size;

    return second;
}

/* Merge a used or free-floating block with its free neighbor. There are two
   parameters for clarity, but really (right == next_block(left)). OOB: nop. */
static void merge(blockptr_t left, blockptr_t right)
{
    block_t *leftp = resolve(left);
    block_t *rightp = resolve(right);
    if(!leftp || !rightp) return;

    size_t extra_size = sizeof(block_t) + rightp->size;
    leftp->last = rightp->last;
    leftp->size += extra_size;

    blockptr_t next = next_block(left);
    block_t *nextp = resolve(next);
    if(nextp) nextp->previous_used = leftp->used;
}

//---
// List-level operations
//---

/* Returns the next free block in the list, assumes (b) is free. OOB: 0; */
static blockptr_t next_link(blockptr_t b)
{
    u32 *footer = block_footer(b);
    return footer ? footer[-1] & ~3 : 0;
}

/* Returns the previous free block in the list, assumes (b) is free. OOB: 0. */
static blockptr_t previous_link(blockptr_t b)
{
    u32 *footer = block_footer(b);
    return footer ? footer[-2] : 0;
}

/* Writes free block links to the footer of free block (b). OOB: nop. */
static void set_footer(
    blockptr_t b, blockptr_t previous_link, blockptr_t next_link)
{
    block_t *bp = resolve(b);
    if(!bp) return;

    u32 *footer = block_footer(b);
    if(!footer) return;

    /* 8-byte block: store the next link with LSB=1 */
    if(bp->size == 8)
    {
        footer[-2] = previous_link;
        footer[-1] = next_link | 1;
    }
    /* Larger block: store the size first then the link */
    else
    {
        footer[-3] = bp->size;
        footer[-2] = previous_link;
        footer[-1] = next_link;
    }
}

/* Find a best fit for the requested size in the list. OOB: 0. */
static blockptr_t best_fit(blockptr_t list, size_t size)
{
    blockptr_t best_match = 0;
    size_t best_size = 0xffffffff;

    while(list && best_size != size)
    {
        block_t *listp = resolve(list);
        if(!listp) return 0;

        if(listp->size >= size && listp->size < best_size)
        {
            best_match = list;
            best_size = listp->size;
        }
        list = next_link(list);
    }

    return best_match;
}

//---
// Index-level operations
//---

/* Returns the size class of the given size */
MQ_INLINE int size_class(size_t size)
{
    if(size < 64) return (size - 8) >> 2;
    if(size < 256) return 14;
    return 15;
}

/* Removes a block from a list, updating the index if needed. The free block is
   in a temporary state of being in no list, called "free-floating". */
static void remove_link(blockptr_t b, index_t *index)
{
    block_t *bp = resolve(b);
    if(!bp) return;
    int c = size_class(bp->size);

    blockptr_t prev = previous_link(b);
    blockptr_t next = next_link(b);

    /* Redirect links around (b) in its list */
    if(prev) set_footer(prev, previous_link(prev), next);
    if(next) set_footer(next, prev, next_link(next));

    if(index->classes[c] == b) index->classes[c] = next;

    index->stats.free_memory -= bp->size;
}

/* Prepends a block to the list for its size class, and update the index */
static void prepend_link(blockptr_t b, index_t *index)
{
    block_t *bp = resolve(b);
    if(!bp) return;
    int c = size_class(bp->size);

    blockptr_t first = index->classes[c];
    set_footer(b, 0, first);
    if(first) set_footer(first, b, next_link(first));

    index->classes[c] = b;

    index->stats.free_memory += bp->size;
}

//---
// Arena allocator
//---

/* Round a size to the closest allocatable size */
static size_t round_size(size_t size)
{
    return (size < 8) ? 8 : ((size + 3) & ~3);
}

u32 mq_heap_malloc(size_t size)
{
    if(!size)
        return 0;

    index_t *index = resolve(mq_heapBase);
    stats_t *s = &index->stats;
    size = round_size(size);
    int c = size_class(size);

    /* Try to find a class that has a free block available */
    blockptr_t alloc = 0;
    for(; c <= 15; c++)
    {
        blockptr_t list = index->classes[c];
        /* The first 14 classes are exact-size, so there is no need to
           search. For the last two, we use a best fit. */
        alloc = (c < 14) ? list : best_fit(list, size);
        if(alloc) break;
    }
    if(!alloc)
    {
        if(s->free_memory >= size) s->fragmentation_failures++;
        if(s->free_memory <  size) s->exhaustion_failures++;
        s->total_failures++;
        return 0;
    }

    block_t *allocp = resolve(alloc);
    if(!allocp) return 0;

    /* Remove the block to allocate from its list */
    remove_link(alloc, index);

    /* If it's larger than needed, split it and reinsert the leftover */
    blockptr_t rest = split(alloc, size);
    if(rest) prepend_link(rest, index);

    /* Mark the block as allocated and return it */
    blockptr_t next = next_block(alloc);
    allocp->used = true;
    block_t *nextp = resolve(next);
    if(nextp) nextp->previous_used = true;

    s->used_memory += allocp->size;
    if(s->used_memory > s->peak_used_memory)
        s->peak_used_memory = s->used_memory;

    s->live_blocks++;
    if(s->live_blocks > s->peak_live_blocks)
        s->peak_live_blocks = s->live_blocks;
    s->total_volume += size;
    s->total_blocks++;
    return alloc + sizeof(block_t);
}

void mq_heap_free(u32 ptr)
{
    if(!ptr)
        return;

    index_t *index = resolve(mq_heapBase);
    blockptr_t b = ptr - sizeof(block_t);
    block_t *bp = resolve(b);

    blockptr_t prev = previous_block_if_free(b);
    blockptr_t next = next_block(b);
    block_t *prevp = resolve(prev);
    block_t *nextp = resolve(next);

    /* Mark the block as free */
    bp->used = false;
    index->stats.live_blocks--;
    index->stats.used_memory -= bp->size;
    if(nextp) nextp->previous_used = false;

    /* Merge with the next block if free */
    if(nextp && !nextp->used)
    {
        remove_link(next, index);
        merge(b, next);
    }
    /* Merge with the previous block if free */
    if(prevp)
    {
        remove_link(prev, index);
        merge(prev, b);
        b = prev;
    }

    /* Insert the result in the index */
    prepend_link(b, index);
}

u32 mq_heap_realloc(u32 ptr, size_t size)
{
    if(!ptr)
        return mq_heap_malloc(size);
    if(!size) {
        mq_heap_free(ptr);
        return 0;
    }

    index_t *index = resolve(mq_heapBase);
    stats_t *s = &index->stats;
    blockptr_t b = ptr - sizeof(block_t);
    block_t *bp = resolve(b);
    size = round_size(size);
    int size_before = bp->size;

    /* When requesting a smaller size, split the original block */
    if(size <= bp->size)
    {
        blockptr_t rest = split(b, size);
        if(rest) {
            /* Try to merge the rest with a following free block */
            blockptr_t next = next_block(rest);
            block_t *nextp = resolve(next);
            if(nextp && !nextp->used)
            {
                remove_link(next, index);
                merge(rest, next);
            }
            prepend_link(rest, index);

            s->used_memory -= (size_before - size);
        }
        s->total_volume += size;
        s->total_blocks++;
        return ptr;
    }

    /* When requesting a larger size and the next block is free and large
       enough, expand the original allocation */
    blockptr_t next = next_block(b);
    block_t *nextp = resolve(next);
    int next_needed = size - bp->size - sizeof(block_t);

    if(nextp && !nextp->used && nextp->size >= next_needed)
    {
        remove_link(next, index);
        blockptr_t rest = split(next, next_needed);
        if(rest) prepend_link(rest, index);
        merge(b, next);

        s->used_memory += (bp->size - size_before);
        s->expanding_reallocs++;
        s->total_volume += size;
        s->total_blocks++;
        return ptr;
    }

    /* Otherwise, perform a brand new allocation */
    u32 new_ptr = mq_heap_malloc(size);
    if(!new_ptr)
    {
        if(size >= s->free_memory) s->exhaustion_failures++;
        if(size <  s->free_memory) s->fragmentation_failures++;
        s->total_failures++;
        return 0;
    }

    /* Move the data and free the original block */
    memcpy(resolve(new_ptr), resolve(ptr), bp->size);
    mq_heap_free(ptr);

    s->relocating_reallocs++;
    s->total_volume += size;
    s->total_blocks++;
    return new_ptr;
}

bool mq_heap_init(u32 start, u32 end, void *buffer)
{
    if(end - start < 256 || !start)
        return false;
    blockptr_t entry_block;
    block_t *entry_blockp;

    mq_heapBase = start;
    mq_heapEnd = end;
    mq_heapBuffer = buffer;

    /* The index is located at the very start of the arena */
    index_t *index = resolve(start);
    memset(index, 0, sizeof *index);
    entry_block = start + sizeof(index_t);
    entry_blockp = resolve(entry_block);

    /* Initialize the first block */
    entry_blockp->last = 1;
    entry_blockp->used = 0;
    entry_blockp->previous_used = 1;
    entry_blockp->size = end - start - sizeof(index_t) - sizeof(block_t);
    set_footer(entry_block, 0, 0);

    /* Initialize the index */
    for(int i = 0; i < 16; i++) index->classes[i] = 0;
    index->classes[size_class(entry_blockp->size)] = entry_block;

    /* Initialize statistics */
    index->stats.free_memory = entry_blockp->size;
    return true;
}

void mq_heap_reset(void)
{
    mq_heapBase = 0;
    mq_heapEnd = 0;
    mq_heapBuffer = NULL;
}

bool mq_heap_isInitialized(u32 *start, u32 *end)
{
    if(!mq_heapBase)
        return false;
    if(start)
        *start = mq_heapBase;
    if(end)
        *end = mq_heapEnd;
    return true;
}

mq_heap_stats_t *mq_heap_stats(void)
{
    index_t *index = resolve(mq_heapBase);
    return &index->stats;
}

//=== Introspection and debugging (also original functions) ==================//

static blockptr_t first_block(void)
{
    return mq_heapBase + sizeof(index_t);
}

int mq_heap_dbg_sequence_length(void)
{
    blockptr_t b = first_block();
    int length = 0;
    while(b) b = next_block(b), length++;
    return length;
}

bool mq_heap_dbg_sequence_covers(void)
{
    blockptr_t b = first_block();
    block_t *bp;
    int total_size = 0;

    while(b >= mq_heapBase && b < mq_heapEnd && (bp = resolve(b)))
    {
        total_size += sizeof(block_t) + bp->size;
        b = next_block(b);
    }

    return (total_size + sizeof(index_t) == mq_heapEnd - mq_heapBase);
}

bool mq_heap_dbg_sequence_terminator(void)
{
    blockptr_t b = first_block();
    block_t *bp;
    while((bp = resolve(b)) && !bp->last) b = next_block(b);
    return bp && b + sizeof(block_t) + bp->size == mq_heapEnd;
}

bool mq_heap_dbg_sequence_coherent_used(void)
{
    blockptr_t b = first_block(), next;
    block_t *bp = resolve(b);
    if(!bp || !bp->previous_used) return false;

    while(b)
    {
        next = next_block(b);
        block_t *nextp = resolve(next);
        bp = resolve(b);
        if(!bp || (next && (!nextp || bp->used != nextp->previous_used)))
            return false;
        b = next;
    }
    return true;
}

bool mq_heap_dbg_sequence_footer_size(void)
{
    for(blockptr_t b = first_block(); b; b = next_block(b))
    {
        block_t *bp = resolve(b);
        if(!bp)
            return false;
        if(bp->used) continue;
        u32 *footer = block_footer(b);
        if(!footer) return false;

        if((footer[-1] & 1) != (bp->size == 8)) return false;
        if(bp->size != 8 && (bp->size != footer[-3])) return false;
    }
    return true;
}

bool mq_heap_dbg_sequence_merged_free(void)
{
    for(blockptr_t b = first_block(); b; b = next_block(b))
    {
        block_t *bp = resolve(b);
        if(!bp)
            return false;
        if(bp->used) continue;
        if(previous_block_if_free(b)) return false;
        blockptr_t next = next_block(b);
        block_t *nextp = resolve(next);
        if(next && (!nextp || !nextp->used)) return false;
    }
    return true;
}

/* Tests for the integrity of the doubly-linked lists */

bool mq_heap_dbg_list_structure(void)
{
    index_t *index = resolve(mq_heapBase);

    for(int c = 0; c < 16; c++)
    {
        blockptr_t b = index->classes[c], next;
        if(!b) continue;
        block_t *bp = resolve(b);
        if(!bp || bp->used) return false;
        if(previous_link(b)) return false;

        while((next = next_link(b)))
        {
            if(previous_link(next) != b) return false;
            b = next;
        }
    }
    return true;
}

/* Tests for the coverage and separation of the segregated lists */

bool mq_heap_dbg_index_covers(void)
{
    index_t *index = resolve(mq_heapBase);
    int32_t total_size = 0;

    for(blockptr_t b = first_block(); b; b = next_block(b))
    {
        block_t *bp = resolve(b);
        if(!bp) return false;
        if(bp->used) total_size += sizeof(block_t) + bp->size;
    }

    for(int c = 0; c < 16; c++)
    for(blockptr_t b = index->classes[c]; b; b = next_link(b))
    {
        block_t *bp = resolve(b);
        if(!bp) return false;
        total_size += sizeof(block_t) + bp->size;
    }

    return (total_size + sizeof(index_t) == mq_heapEnd - mq_heapBase);
}

bool mq_heap_dbg_index_class_separation(void)
{
    index_t *index = resolve(mq_heapBase);

    for(int c = 0; c < 16; c++)
    for(blockptr_t b = index->classes[c]; b; b = next_link(b))
    {
        block_t *bp = resolve(b);
        if(!bp) return false;
        if(size_class(bp->size) != c) return false;
    }
    return true;
}

mq_heap_debug_t mq_heap_debuginfo(void)
{
    mq_heap_debug_t dbg = { 0 };
    if(!mq_heap_isInitialized(NULL, NULL))
        return dbg;

    dbg.sequence_covers = mq_heap_dbg_sequence_covers();
    dbg.sequence_terminator = mq_heap_dbg_sequence_terminator();
    dbg.sequence_coherent_used = mq_heap_dbg_sequence_coherent_used();
    dbg.sequence_footer_size = mq_heap_dbg_sequence_footer_size();
    dbg.sequence_merged_free = mq_heap_dbg_sequence_merged_free();
    dbg.list_structure = mq_heap_dbg_list_structure();
    dbg.index_covers = mq_heap_dbg_index_covers();
    dbg.index_class_separation = mq_heap_dbg_index_class_separation();
    return dbg;
}
