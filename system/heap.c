//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

/* The following is a copy of gint's allocator, sparsely modified. The main
   changes are removing some gint-exclusive features, allowing stats to be read
   from the emulator side, and distinguishing "emulated pointers" (u32) from
   "host pointers" (void *), the conversion between which is given by the
   mq_resolvePointer() and mq_createPointer() below. Note that the internal
   layout is host-endian and may absolutely not be the same as the layout on
   calculator. This is not a problem because the heap's internal details are
   hidden by the syscall interface and add-ins shouldn't be looking there. */

#include <mq/system/heap.h>
#include <mq/memory.h>
#include <string.h>

/* Globals to avoid modifying the allocator code too much. */
static u32 mq_heapBase = 0;
static u32 mq_heapEnd = 0;
static void *mq_heapBuffer = NULL;

static void *mq_resolvePointer(u32 address)
{
    if(address == 0)
        return NULL;
    return mq_heapBuffer + (address - mq_heapBase);
}

static u32 mq_createPointer(void volatile const *ptr)
{
    if(ptr == NULL)
        return 0;
    return mq_heapBase + (ptr - mq_heapBuffer);
}

//=== (Mostly) original gint allocator =======================================//
// The comments below are original and not related to MQ.

/* block_t: A memory block managed by the heap.

   The heap is a sequence of blocks made of a block_t header (4 bytes) and raw
   data (any size between 8 bytes and the size of the heap). The sequence
   extends from the start of the arena region (past the index structure) up to
   the end of the arena region.

   Free blocks use the unused raw data to store a footer of either 8 or 12
   bytes, which links to the start of the block, the previous free block, and
   the next free block. This forms a doubly-linked list of free blocks (or, to
   be more precise, several intertwined doubly-linked lists, each handling a
   different class of block sizes).

   The possible ways to traverse the structure are:
   1. Traverse the sequence from left to right -> next_block()
   2. Go back to the previous block, if it's free -> previous_block_if_free()
   3. Traverse each linked list from left to right -> next_link()
   4. Traverse each linked list from right to left -> previous_link()

   Way #2 is implemented using the boundary tag optimization. Basically each
   block has a bit (the boundary tag) that tells whether the previous block is
   free. If it's free, then that block's footer can be accessed, and because
   the footer contains the size the header can be accessed too. This is used to
   detect whether to merge into the previous block after a free().

   The allocation algorithm will mostly use way #3 to find free blocks. When
   freeing, ways #1 and #2 are used to coalesce adjacent blocks. Ways #3 and #4
   are used to maintain the linked lists.

   The footer uses 8 bytes if the block has 8 bytes of raw data, and 12 bytes
   otherwise. The LSB of the last byte is used to distinguish the cases:
   * For a block of 8 bytes, the footer has one block_t pointer to the previous
     link, then one block_t pointer to the next link with LSB=1
   * For a larger block, the footer has a 4-byte block size, then a pointer to
     the previous link, and a pointer to the next link with LSB=0. */
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

typedef mq_heap_stats_t stats_t;

/* index_t: Data structure at the root of the heap, indexing linked lists */
typedef struct {
    /* Entry points of the free lists for each block size */
    block_t *classes[16];
    /* Pointer to statistics, if used */
    stats_t stats;
} index_t;

//---
// Block-level operations
//---

/* Returns a pointer to the next block in the sequence (might be used) */
static block_t *next_block(block_t *b)
{
    if(b->last) return NULL;
    return (void *)b + sizeof(block_t) + b->size;
}

/* Returns a pointer to the previous block's header, if it's a free block */
static block_t *previous_block_if_free(block_t *b)
{
    if(b->previous_used) return NULL;
    /* The footer of the previous block indicates its size */
    uint32_t *footer = (void *)b;
    uint32_t previous_size = (footer[-1] & 1) ? 8 : footer[-3];
    return (void *)b - previous_size - sizeof(block_t);
}

/* Splits a used or free-floating block into a first block with (size) bytes
   and a free second block with the rest. Returns the address of the second
   block. If the initial block is too small to split, returns NULL. */
static block_t *split(block_t *b, int size)
{
    size_t extra_size = b->size - size;
    if(extra_size < sizeof(block_t) + 8) return NULL;

    block_t *second = (void *)b + sizeof(block_t) + size;
    second->last = b->last;
    second->used = false;
    second->previous_used = b->used;
    second->size = extra_size - sizeof(block_t);

    block_t *third = next_block(second);
    if(third) third->previous_used = second->used;

    b->last = 0;
    b->size = size;

    return second;
}

/* Merge a used or free-floating block with its free neighbor. There are two
   parameters for clarity, but really (right == next_block(left)). */
static void merge(block_t *left, block_t *right)
{
    size_t extra_size = sizeof(block_t) + right->size;
    left->last = right->last;
    left->size += extra_size;

    block_t *next = next_block(left);
    if(next) next->previous_used = left->used;
}

//---
// List-level operations
//---

/* Returns the next free block in the list, assumes (b) is free */
static block_t *next_link(block_t *b)
{
    uint32_t *footer = (void *)b + sizeof(block_t) + b->size;
    return mq_resolvePointer(footer[-1] & ~3);
}

/* Returns the previous free block in the list, assumes (b) is free */
static block_t *previous_link(block_t *b)
{
    uint32_t *footer = (void *)b + sizeof(block_t) + b->size;
    return mq_resolvePointer(footer[-2]);
}

/* Writes the given free block links to the footer of free block (b) */
static void set_footer(block_t *b, block_t *previous_link, block_t *next_link)
{
    uint32_t *footer = (void *)b + sizeof(block_t) + b->size;
    /* 8-byte block: store the next link with LSB=1 */
    if(b->size == 8)
    {
        footer[-2] = mq_createPointer(previous_link);
        footer[-1] = mq_createPointer(next_link) | 1;
    }
    /* Larger block: store the size first then the link */
    else
    {
        footer[-3] = b->size;
        footer[-2] = mq_createPointer(previous_link);
        footer[-1] = mq_createPointer(next_link);
    }
}

/* Find a best fit for the requested size in the list */
static block_t *best_fit(block_t *list, size_t size)
{
    block_t *best_match = NULL;
    size_t best_size = 0xffffffff;

    while(list && best_size != size)
    {
        if(list->size >= size && list->size < best_size)
        {
            best_match = list;
            best_size = list->size;
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
   in a temporary state of being in no list, called "free-floating" */
static void remove_link(block_t *b, index_t *index)
{
    int c = size_class(b->size);

    block_t *prev = previous_link(b);
    block_t *next = next_link(b);

    /* Redirect links around (b) in its list */
    if(prev) set_footer(prev, previous_link(prev), next);
    if(next) set_footer(next, prev, next_link(next));

    if(index->classes[c] == b) index->classes[c] = next;

    index->stats.free_memory -= b->size;
}

/* Prepends a block to the list for its size class, and update the index */
static void prepend_link(block_t *b, index_t *index)
{
    int c = size_class(b->size);

    block_t *first = index->classes[c];
    set_footer(b, NULL, first);
    if(first) set_footer(first, b, next_link(first));

    index->classes[c] = b;

    index->stats.free_memory += b->size;
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
        return mq_createPointer(NULL);

    index_t *index = mq_resolvePointer(mq_heapBase);
    stats_t *s = &index->stats;
    size = round_size(size);
    int c = size_class(size);

    /* Try to find a class that has a free block available */
    block_t *alloc = NULL;
    for(; c <= 15; c++)
    {
        block_t *list = index->classes[c];
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
        return mq_createPointer(NULL);
    }

    /* Remove the block to allocate from its list */
    remove_link(alloc, index);

    /* If it's larger than needed, split it and reinsert the leftover */
    block_t *rest = split(alloc, size);
    if(rest) prepend_link(rest, index);

    /* Mark the block as allocated and return it */
    block_t *next = next_block(alloc);
    alloc->used = true;
    if(next) next->previous_used = true;

    s->used_memory += alloc->size;
    if(s->used_memory > s->peak_used_memory)
        s->peak_used_memory = s->used_memory;

    s->live_blocks++;
    if(s->live_blocks > s->peak_live_blocks)
        s->peak_live_blocks = s->live_blocks;
    s->total_volume += size;
    s->total_blocks++;
    return mq_createPointer((void *)alloc + sizeof(block_t));
}

void mq_heap_free(u32 ptr32)
{
    if(!ptr32)
        return;

    void *ptr = mq_resolvePointer(ptr32);
    index_t *index = mq_resolvePointer(mq_heapBase);
    block_t *b = ptr - sizeof(block_t);

    block_t *prev = previous_block_if_free(b);
    block_t *next = next_block(b);

    /* Mark the block as free */
    b->used = false;
    index->stats.live_blocks--;
    index->stats.used_memory -= b->size;
    if(next) next->previous_used = false;

    /* Merge with the next block if free */
    if(next && !next->used)
    {
        remove_link(next, index);
        merge(b, next);
    }
    /* Merge with the previous block if free */
    if(prev)
    {
        remove_link(prev, index);
        merge(prev, b);
        b = prev;
    }

    /* Insert the result in the index */
    prepend_link(b, index);
}

u32 mq_heap_realloc(u32 ptr32, size_t size)
{
    if(!ptr32)
        return mq_heap_malloc(size);
    if(!size) {
        mq_heap_free(ptr32);
        return mq_createPointer(NULL);
    }

    void *ptr = mq_resolvePointer(ptr32);
    index_t *index = mq_resolvePointer(mq_heapBase);
    stats_t *s = &index->stats;
    block_t *b = ptr - sizeof(block_t);
    size = round_size(size);
    int size_before = b->size;

    /* When requesting a smaller size, split the original block */
    if(size <= b->size)
    {
        block_t *rest = split(b, size);
        if(rest) {
            /* Try to merge the rest with a following free block */
            block_t *next = next_block(rest);
            if(next && !next->used)
            {
                remove_link(next, index);
                merge(rest, next);
            }
            prepend_link(rest, index);

            s->used_memory -= (size_before - size);
        }
        s->total_volume += size;
        s->total_blocks++;
        return mq_createPointer(ptr);
    }

    /* When requesting a larger size and the next block is free and large
       enough, expand the original allocation */
    block_t *next = next_block(b);
    int next_needed = size - b->size - sizeof(block_t);

    if(next && !next->used && next->size >= next_needed)
    {
        remove_link(next, index);
        block_t *rest = split(next, next_needed);
        if(rest) prepend_link(rest, index);
        merge(b, next);

        s->used_memory += (b->size - size_before);
        s->expanding_reallocs++;
        s->total_volume += size;
        s->total_blocks++;
        return mq_createPointer(ptr);
    }

    /* Otherwise, perform a brand new allocation */
    void *new_ptr = mq_resolvePointer(mq_heap_malloc(size));
    if(!new_ptr)
    {
        if(size >= s->free_memory) s->exhaustion_failures++;
        if(size <  s->free_memory) s->fragmentation_failures++;
        s->total_failures++;
        return mq_createPointer(NULL);
    }

    /* Move the data and free the original block */
    memcpy(new_ptr, ptr, b->size);
    mq_heap_free(ptr32);

    s->relocating_reallocs++;
    s->total_volume += size;
    s->total_blocks++;
    return mq_createPointer(new_ptr);
}

bool mq_heap_init(u32 start, u32 end, void *buffer)
{
    if(end - start < 256 || !start)
        return false;
    block_t *entry_block;

    mq_heapBase = start;
    mq_heapEnd = end;
    mq_heapBuffer = buffer;

    /* The index is located at the very start of the arena */
    index_t *index = mq_resolvePointer(start);
    memset(index, 0, sizeof *index);
    entry_block = mq_resolvePointer(start + sizeof(index_t));

    /* Initialize the first block */
    entry_block->last = 1;
    entry_block->used = 0;
    entry_block->previous_used = 1;
    entry_block->size = end - start - sizeof(index_t) - sizeof(block_t);
    set_footer(entry_block, NULL, NULL);

    /* Initialize the index */
    for(int i = 0; i < 16; i++) index->classes[i] = NULL;
    index->classes[size_class(entry_block->size)] = entry_block;

    /* Initialize statistics */
    index->stats.free_memory = entry_block->size;
    return true;
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
    index_t *index = mq_resolvePointer(mq_heapBase);
    return &index->stats;
}

//=== Introspection and debugging (also original functions) ==================//

static block_t *first_block(void)
{
    return mq_resolvePointer(mq_heapBase + sizeof(index_t));
}

int mq_heap_dbg_sequence_length(void)
{
    block_t *b = first_block();
    int length = 0;
    while(b) b = next_block(b), length++;
    return length;
}

bool mq_heap_dbg_sequence_covers(void)
{
    block_t *b = first_block();
    int total_size = 0;

    while(mq_createPointer(b) >= mq_heapBase
          && mq_createPointer(b) < mq_heapEnd)
    {
        total_size += sizeof(block_t) + b->size;
        b = next_block(b);
    }

    return (total_size + sizeof(index_t) == mq_heapEnd - mq_heapBase);
}

bool mq_heap_dbg_sequence_terminator(void)
{
    block_t *b = first_block();
    while(!b->last) b = next_block(b);
    return (mq_createPointer(b) + sizeof(block_t) + b->size == mq_heapEnd);
}

bool mq_heap_dbg_sequence_coherent_used(void)
{
    block_t *b = first_block(), *next;
    if(!b->previous_used) return false;

    while(b)
    {
        next = next_block(b);
        if(next && b->used != next->previous_used) return false;
        b = next;
    }
    return true;
}

bool mq_heap_dbg_sequence_footer_size(void)
{
    for(block_t *b = first_block(); b; b = next_block(b))
    {
        if(b->used) continue;
        uint32_t *footer = (void *)b + sizeof(block_t) + b->size;

        if((footer[-1] & 1) != (b->size == 8)) return false;
        if(b->size != 8 && (b->size != footer[-3])) return false;
    }
    return true;
}

bool mq_heap_dbg_sequence_merged_free(void)
{
    for(block_t *b = first_block(); b; b = next_block(b))
    {
        if(b->used) continue;
        if(previous_block_if_free(b)) return false;
        if(next_block(b) && !next_block(b)->used) return false;
    }
    return true;
}

/* Tests for the integrity of the doubly-linked lists */

bool mq_heap_dbg_list_structure(void)
{
    index_t *index = mq_resolvePointer(mq_heapBase);

    for(int c = 0; c < 16; c++)
    {
        block_t *b = index->classes[c], *next;
        if(!b) continue;
        if(b->used) return false;
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
    index_t *index = mq_resolvePointer(mq_heapBase);
    int32_t total_size = 0;

    for(block_t *b = first_block(); b; b = next_block(b))
    {
        if(b->used) total_size += sizeof(block_t) + b->size;
    }

    for(int c = 0; c < 16; c++)
    for(block_t *b = index->classes[c]; b; b = next_link(b))
    {
        total_size += sizeof(block_t) + b->size;
    }

    return (total_size + sizeof(index_t) == mq_heapEnd - mq_heapBase);
}

bool mq_heap_dbg_index_class_separation(void)
{
    index_t *index = mq_resolvePointer(mq_heapBase);

    for(int c = 0; c < 16; c++)
    for(block_t *b = index->classes[c]; b; b = next_link(b))
    {
        if(size_class(b->size) != c) return false;
    }
    return true;
}

