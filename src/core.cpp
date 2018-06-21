#include <melon/core.hpp>
#include <string.h>

namespace melon
{

////////////////////////////////////////////////////////////////////////////////
// Default memory allocation callbacks
////////////////////////////////////////////////////////////////////////////////

static void *tz_malloc(void *user_data, size_t size, size_t align)
{
    size_t offset = align + sizeof(void *);
    void *ptr = malloc(size + offset);

    if (ptr == NULL)
        return NULL;

    void **return_ptr = (void **)tz_align_forward(((void **)ptr) + 1, align);
    return_ptr[-1] = ptr;

    return (void *)return_ptr;
}

static void *tz_realloc(void *user_data, void *ptr, size_t size, size_t align)
{
    size_t offset = align + sizeof(void *);
    void *new_ptr = realloc(ptr, size + offset);

    if (new_ptr == NULL)
        return NULL;

    void **return_ptr = (void **)tz_align_forward(((void **)new_ptr) + 1, align);
    return_ptr[-1] = new_ptr;

    return (void *)return_ptr;
}

static void tz_free(void *user_data, void *ptr)
{
    free(((void **)ptr)[-1]);
}

const tz_allocator *tz_default_cb_allocator()
{
    static tz_allocator *p_allocator = NULL;
    static tz_allocator allocator;

    if (p_allocator == NULL)
    {
        allocator.alloc = tz_malloc;
        allocator.realloc = tz_realloc;
        allocator.dealloc = tz_free;
        allocator.user_data = NULL;

        p_allocator = &allocator;
    }

    return p_allocator;
}

////////////////////////////////////////////////////////////////////////////////
// Arena functions
////////////////////////////////////////////////////////////////////////////////

tz_arena tz_create_arena(tz_memory_block *prev, uint32_t alloc_flags, size_t size, size_t align, const tz_allocator *alloc)
{
    tz_memory_block *result = alloc->alloc(alloc->user_data, size + align + sizeof(tz_memory_block), align);
    result->start = (uint8_t *)tz_align_forward(result + 1, align);
    result->offset = 0;
    result->size = size;
    result->allocator = *alloc;
    result->prev = prev;

    tz_arena arena;
    arena.current_block = result;

    return arena;
}

void tz_destroy_arena(tz_arena *arena)
{
    while (arena->current_block)
    {
        tz_memory_block *prev = arena->current_block->prev;
        TZ_FREE(arena->current_block->allocator, arena->current_block);

        arena->current_block = prev;
    }
}

void *tz_arena_push_size(tz_arena *arena, size_t size, size_t align)
{
    // Try to increment offset on current block
    tz_memory_block *block = arena->current_block;
    uint8_t *result = (uint8_t *)tz_align_forward(block->start + block->offset, align);
    size_t offset = result - block->start + size;

    // If the new offset is within the block, return the new pointer
    if (offset < block->size)
    {
        block->offset = offset;
        return (void *)result;
    }

    // Allocate a new block
    size_t new_block_size = arena->allocation_flags & TZ_ALLOC_EXPAND_DOUBLE ? block->size * 2 : block->size;
    while (size > new_block_size)
        new_block_size *= 2;
    new_block_size += align;

    *arena = tz_create_arena(block, arena->allocation_flags, new_block_size, align, &block->allocator);
    tz_memory_block *new_block = arena->current_block;

    result = (uint8_t *)tz_align_forward(new_block->start, align);
    new_block->offset = tz_aligned_size(new_block->start, new_block->offset + size, align);

    return result;
}

// Deallocate extra blocks, reset offset to 0
void tz_arena_reset(tz_arena *arena)
{
    tz_memory_block *current_block = arena->current_block;
    while (current_block->prev)
    {
        TZ_FREE(current_block->allocator, current_block);
    }
    current_block->offset = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Default logging callbacks
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

static void tz_default_logger(const char *message, ...)
{
    va_list arg_list;
    va_start(arg_list, message);
    vfprintf(stderr, message, arg_list);
    va_end(arg_list);
}

tz_cb_logger tz_logger_callback = tz_default_logger;

////////////////////////////////////////////////////////////////////////////////
// Pool implementation
////////////////////////////////////////////////////////////////////////////////

void tz_create_pool(tz_pool *pool, size_t capacity, const tz_allocator *allocator)
{
    pool->allocator = *allocator;
    pool->free_indices = (size_t *)TZ_ALLOC(pool->allocator, sizeof(size_t) * capacity, TZ_DEFAULT_ALIGN);
    pool->capacity = capacity;
    pool->num_free_indices = capacity;

    for (size_t i = 0; i < capacity; i++)
        pool->free_indices[capacity - 1 - i] = i;
}

void tz_delete_pool(tz_pool *pool)
{
    TZ_FREE(pool->allocator, pool->free_indices);
}

tz_pool_index tz_pool_create_index(tz_pool *pool)
{
    tz_pool_index new_index = tz_pool_gen_invalid_index();

    uint32_t index;

    // Pop free indices off the stack until one with a valid generation is found
    if (pool->num_free_indices == 0)
    {
        // Reallocate
        size_t new_capacity = pool->capacity * 2;
        pool->free_indices = TZ_REALLOC(pool->allocator, pool->free_indices, sizeof(size_t) * new_capacity, TZ_DEFAULT_ALIGN);

        for (size_t i = pool->capacity; i < new_capacity; i++)
            pool->free_indices[i - pool->capacity] = new_capacity - 1 - i;

        pool->capacity = new_capacity;
        pool->num_free_indices = new_capacity;
    }

    index = pool->free_indices[--pool->num_free_indices];

    new_index = index;

    return new_index;
}

bool tz_pool_index_is_valid(tz_pool *pool, tz_pool_index index)
{
    return (index < pool->capacity);
}

tz_pool_index tz_pool_gen_invalid_index()
{
    return (tz_pool_index){0};
}

bool tz_pool_delete_index(tz_pool *pool, tz_pool_index index)
{
    if (!tz_pool_index_is_valid(pool, index))
        return false;

    // if the number of free indices >= the capacity, the pool is empty
    if (pool->num_free_indices >= pool->capacity)
        return false;

#ifdef TZ_DEBUG
    for (size_t i = 0; i < pool->num_free_indices; i++)
        TZ_ASSERT(pool->free_indices[i] != index, "Cannot double free indices\n");
#endif

    pool->free_indices[pool->num_free_indices++] = index;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// tz_pool_vector - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

void tz_create_pool_vector(tz_pool_vector *pv, size_t capacity, size_t element_size, const tz_allocator *allocator)
{
    pv->allocator = *allocator;
    pv->capacity = capacity;
    pv->element_size = element_size;

    tz_create_pool(&pv->pool, capacity, allocator);
    pv->data = TZ_ALLOC(pv->allocator, capacity * element_size, element_size);
}

void tz_delete_pool_vector(tz_pool_vector *pv)
{
    tz_delete_pool(&pv->pool);
    TZ_FREE(pv->allocator, pv->data);
}

tz_pool_index tz_pool_vector_push(tz_pool_vector *pv, void *val)
{
    tz_pool_index index = tz_pool_create_index(&pv->pool);

    if (index < pv->capacity)
    {
        memcpy((uint8_t *)pv->data + pv->element_size * index, val, pv->element_size);
        return index;
    }

    size_t new_capacity = pv->capacity * 2;

    while (new_capacity <= index)
        new_capacity *= 2;

    pv->data = TZ_REALLOC(pv->allocator, pv->data, new_capacity, pv->element_size);
    memcpy((uint8_t *)pv->data + pv->element_size * index, val, pv->element_size);

    return index;
}
} // namespace melon