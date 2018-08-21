#include <melon/core/memory.h>
#include <melon/core/error.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Default memory allocation callbacks
////////////////////////////////////////////////////////////////////////////////

static void* aligned_malloc(void* user_data, size_t size, size_t align)
{
    size_t offset = align - 1 + sizeof(void*);
    void*  ptr    = malloc(size + offset);

    if (ptr == NULL)
    {
        return NULL;
    }

    void** return_ptr = (void**) melon_align_forward(((void**) ptr) + 1, align);
    return_ptr[-1]    = ptr;

    return (void*) return_ptr;
}

static void* aligned_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
    size_t offset  = align - 1 + sizeof(void*);
    void*  old_ptr = ((void**) ptr)[-1];
    void*  new_ptr = realloc(old_ptr, size + offset);

    if (new_ptr == NULL)
    {
        return NULL;
    }

    if (new_ptr == old_ptr)
    {
        return ptr;
    }

    void** return_ptr = (void**) melon_align_forward(((void**) new_ptr) + 1, align);
    return_ptr[-1]    = new_ptr;

    return (void*) return_ptr;
}

static void aligned_free(void* user_data, void* ptr) { free(((void**) ptr)[-1]); }

const melon_allocator_api* melon_default_cb_allocator()
{
    static melon_allocator_api* p_allocator = NULL;
    static melon_allocator_api  allocator;

    if (p_allocator == NULL)
    {
        allocator.alloc     = aligned_malloc;
        allocator.realloc   = aligned_realloc;
        allocator.dealloc   = aligned_free;
        allocator.user_data = NULL;

        p_allocator = &allocator;
    }

    return p_allocator;
}

////////////////////////////////////////////////////////////////////////////////
// Arena functions
////////////////////////////////////////////////////////////////////////////////

melon_memory_arena melon_create_arena_appended(melon_memory_block* prev, uint32_t melon_alloc_flags, size_t size,
                                               size_t align, const melon_allocator_api* alloc)
{
    melon_memory_block* result
        = (melon_memory_block*) MELON_ALLOC((*alloc), size + align + sizeof(melon_memory_block), align);
    result->start     = (uint8_t*) melon_align_forward(result + 1, align);
    result->offset    = 0;
    result->size      = size;
    result->allocator = *alloc;
    result->prev      = prev;

    melon_memory_arena arena;
    arena.current_block = result;

    return arena;
}

void melon_destroy_arena(melon_memory_arena* arena)
{
    while (arena->current_block)
    {
        melon_memory_block* prev = arena->current_block->prev;
        MELON_FREE(arena->current_block->allocator, arena->current_block);

        arena->current_block = prev;
    }
}

void* melon_arena_push_size(melon_memory_arena* arena, size_t size, size_t align)
{
    // Try to increment offset on current block
    melon_memory_block* block  = arena->current_block;
    uint8_t*            result = (uint8_t*) melon_align_forward(block->start + block->offset, align);
    size_t              offset = result - block->start + size;

    // If the new offset is within the block, return the new pointer
    if (offset < block->size)
    {
        block->offset = offset;
        return (void*) result;
    }

    // Allocate a new block
    size_t new_block_size = arena->allocation_flags & MELON_ALLOC_EXPAND_DOUBLE ? block->size * 2 : block->size;
    while (size > new_block_size)
        new_block_size *= 2;
    new_block_size += align;

    *arena = melon_create_arena_appended(block, arena->allocation_flags, new_block_size, align, &block->allocator);
    melon_memory_block* new_block = arena->current_block;

    result            = (uint8_t*) melon_align_forward(new_block->start, align);
    new_block->offset = melon_aligned_size(new_block->start, new_block->offset + size, align);

    return result;
}

// Deallocate extra blocks, reset offset to 0
void melon_arena_reset(melon_memory_arena* arena)
{
    melon_memory_block* current_block = arena->current_block;
    while (current_block->prev)
    {
        MELON_FREE(current_block->allocator, current_block);
    }
    current_block->offset = 0;
}
