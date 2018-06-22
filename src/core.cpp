#include <melon/core.hpp>
#include <string.h>

namespace melon
{
////////////////////////////////////////////////////////////////////////////////
// Default memory allocation callbacks
////////////////////////////////////////////////////////////////////////////////

static void* aligned_malloc(void* user_data, size_t size, size_t align)
{
    size_t offset = align + sizeof(void*);
    void*  ptr    = malloc(size + offset);

    if (ptr == NULL)
        return NULL;

    void** return_ptr = (void**) align_forward(((void**) ptr) + 1, align);
    return_ptr[-1]    = ptr;

    return (void*) return_ptr;
}

static void* aligned_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
    size_t offset  = align + sizeof(void*);
    void*  new_ptr = realloc(ptr, size + offset);

    if (new_ptr == NULL)
        return NULL;

    void** return_ptr = (void**) align_forward(((void**) new_ptr) + 1, align);
    return_ptr[-1]    = new_ptr;

    return (void*) return_ptr;
}

static void aligned_free(void* user_data, void* ptr) { free(((void**) ptr)[-1]); }

const allocator* default_cb_allocator()
{
    static allocator* p_allocator = NULL;
    static allocator  allocator;

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

memory_arena create_arena(memory_block* prev, uint32_t alloc_flags, size_t size, size_t align, const allocator* alloc)
{
    memory_block* result = (memory_block*) MELON_ALLOC((*alloc), size + align + sizeof(memory_block), align);
    result->start        = (uint8_t*) align_forward(result + 1, align);
    result->offset       = 0;
    result->size         = size;
    result->allocator    = *alloc;
    result->prev         = prev;

    memory_arena arena;
    arena.current_block = result;

    return arena;
}

void destroy_arena(memory_arena* arena)
{
    while (arena->current_block)
    {
        memory_block* prev = arena->current_block->prev;
        MELON_FREE(arena->current_block->allocator, arena->current_block);

        arena->current_block = prev;
    }
}

void* arena_push_size(memory_arena* arena, size_t size, size_t align)
{
    // Try to increment offset on current block
    memory_block* block  = arena->current_block;
    uint8_t*      result = (uint8_t*) align_forward(block->start + block->offset, align);
    size_t        offset = result - block->start + size;

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

    *arena                  = create_arena(block, arena->allocation_flags, new_block_size, align, &block->allocator);
    memory_block* new_block = arena->current_block;

    result            = (uint8_t*) align_forward(new_block->start, align);
    new_block->offset = aligned_size(new_block->start, new_block->offset + size, align);

    return result;
}

// Deallocate extra blocks, reset offset to 0
void arena_reset(memory_arena* arena)
{
    memory_block* current_block = arena->current_block;
    while (current_block->prev)
    {
        MELON_FREE(current_block->allocator, current_block);
    }
    current_block->offset = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Default logging callbacks
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

static void default_logger(const char* message, ...)
{
    va_list arg_list;
    va_start(arg_list, message);
    vfprintf(stderr, message, arg_list);
    va_end(arg_list);
}

logger_callback_fp logger_callback = default_logger;

////////////////////////////////////////////////////////////////////////////////
// Pool implementation
////////////////////////////////////////////////////////////////////////////////

void create_pool(index_pool* pool, size_t capacity, const allocator* allocator)
{
    pool->allocator        = *allocator;
    pool->free_indices     = (size_t*) MELON_ALLOC(pool->allocator, sizeof(size_t) * capacity, MELON_DEFAULT_ALIGN);
    pool->capacity         = capacity;
    pool->num_free_indices = capacity;

    for (size_t i = 0; i < capacity; i++)
        pool->free_indices[capacity - 1 - i] = i;
}

void delete_pool(index_pool* pool) { MELON_FREE(pool->allocator, pool->free_indices); }

pool_index pool_create_index(index_pool* pool)
{
    pool_index new_index = pool_gen_invalid_index();

    pool_index index;

    // Pop aligned_free indices off the stack until one with a valid
    // generation is found
    if (pool->num_free_indices == 0)
    {
        // Reallocate
        size_t new_capacity = pool->capacity * 2;
        pool->free_indices = (size_t*) MELON_REALLOC(pool->allocator, pool->free_indices, sizeof(size_t) * new_capacity,
                                                     MELON_DEFAULT_ALIGN);

        for (size_t i = pool->capacity; i < new_capacity; i++)
            pool->free_indices[i - pool->capacity] = new_capacity - 1 - i;

        pool->capacity         = new_capacity;
        pool->num_free_indices = new_capacity;
    }

    index = pool->free_indices[--pool->num_free_indices];

    new_index = index;

    return new_index;
}

bool pool_index_is_valid(index_pool* pool, pool_index index) { return (index < pool->capacity); }

pool_index pool_gen_invalid_index() { return { 0 }; }

bool pool_delete_index(index_pool* pool, pool_index index)
{
    if (!pool_index_is_valid(pool, index))
        return false;

    // if the number of aligned_free indices >= the capacity, the pool is
    // empty
    if (pool->num_free_indices >= pool->capacity)
        return false;

#ifdef DEBUG
    for (size_t i = 0; i < pool->num_free_indices; i++)
        ASSERT(pool->free_indices[i] != index, "Cannot double aligned_free indices\n");
#endif

    pool->free_indices[pool->num_free_indices++] = index;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// pool_vector - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

void create_pool_vector(pool_vector* pv, size_t capacity, size_t element_size, const allocator* allocator)
{
    pv->allocator    = *allocator;
    pv->capacity     = capacity;
    pv->element_size = element_size;

    create_pool(&pv->pool, capacity, allocator);
    pv->data = MELON_ALLOC(pv->allocator, capacity * element_size, element_size);
}

void delete_pool_vector(pool_vector* pv)
{
    delete_pool(&pv->pool);
    MELON_FREE(pv->allocator, pv->data);
}

pool_index pool_vector_push(pool_vector* pv, void* val)
{
    pool_index index = pool_create_index(&pv->pool);

    if (index < pv->capacity)
    {
        memcpy((uint8_t*) pv->data + pv->element_size * index, val, pv->element_size);
        return index;
    }

    size_t new_capacity = pv->capacity * 2;

    while (new_capacity <= index)
        new_capacity *= 2;

    pv->data = MELON_REALLOC(pv->allocator, pv->data, new_capacity, pv->element_size);
    memcpy((uint8_t*) pv->data + pv->element_size * index, val, pv->element_size);

    return index;
}
}    // namespace melon