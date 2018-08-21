#include <melon/core/handle.h>
#include <melon/core/error.h>
#include <string.h>
#include <stdbool.h>

#define handle_index(handle) melon_handle_index(handle)

static inline uint64_t handle_generation(melon_handle handle)
{
    return ((handle & MELON_HANDLE_GENERATION_MASK) >> MELON_HANDLE_INDEX_BITS);
}

static inline void handle_set_index(melon_handle* handle, uint64_t index)
{
    MELON_ASSERT(index <= MELON_HANDLE_INDEX_MAX);
    (*handle) = index | ((*handle) | MELON_HANDLE_GENERATION_MASK);
}

static inline void handle_set_generation(melon_handle* handle, uint64_t generation)
{
    MELON_ASSERT(generation <= MELON_HANDLE_GENERATION_MAX);
    (*handle) = (generation << MELON_HANDLE_GENERATION_BITS) | ((*handle) | MELON_HANDLE_INDEX_MASK);
}

static inline void handle_increment_generation(melon_handle* handle)
{
    MELON_ASSERT(handle_generation(*handle) + 1 <= MELON_HANDLE_GENERATION_MAX);
    (*handle) += ((melon_handle) 1 << MELON_HANDLE_GENERATION_BITS);
}

static inline bool freelist_empty(melon_handle_pool* pool)
{
    return pool->freelist_tail_index == MELON_HANDLE_INDEX_INVALID && pool->freelist_head_index == MELON_HANDLE_INDEX_INVALID;
}


////////////////////////////////////////////////////////////////////////////////
// Pool implementation
////////////////////////////////////////////////////////////////////////////////

static bool push_free_handle(melon_handle_pool* pool, melon_handle handle)
{
    // Get the index of the handle
    uint64_t index = handle_index(handle);

    // Find the handle entry, update the data index and set the next handle index to be the sentinal
    melon_handle_entry* new_tail = &pool->handle_entries[index];

    // Check to see if the index has expired
    // TODO: Log that the index has expired
    if (handle_generation(handle) >= MELON_HANDLE_GENERATION_MAX)
    {
        new_tail->handle = MELON_INVALID_HANDLE;
        new_tail->next_handle_index = MELON_HANDLE_INDEX_INVALID;
        return false;
    }
    new_tail->handle = handle;
    new_tail->next_handle_index = MELON_HANDLE_INDEX_INVALID;

    // If the freelist is empty, make handle the head
    if (freelist_empty(pool))
    {
        pool->freelist_head_index = index;
    }
    else
    {
        melon_handle_entry* tail      = &pool->handle_entries[pool->freelist_tail_index];
        tail->next_handle_index = index;
    }

    // Set the tail index of the freelist to the new tail index
    pool->freelist_tail_index = index;

    return true;
}

static melon_handle pop_free_handle(melon_handle_pool* pool)
{
    if (freelist_empty(pool))
    {
        return MELON_INVALID_HANDLE;
    }

    // Get the current head entry
    melon_handle_entry* head = &pool->handle_entries[pool->freelist_head_index];

    // If the head and the tail are the same (ie there's only one element in the freelist)
    // make both the tail and the head invalid
    if (pool->freelist_head_index == pool->freelist_tail_index)
    {
        pool->freelist_head_index = MELON_HANDLE_INDEX_INVALID;
        pool->freelist_tail_index = MELON_HANDLE_INDEX_INVALID;
        return head->handle;
    }
    else
    {
        // Else, make the new head the next handle index
        pool->freelist_head_index = head->next_handle_index;
        return head->handle;
    }
}

void melon_create_handle_pool(melon_handle_pool* pool, size_t capacity, const melon_allocator_api* allocator, bool grow_by_default)
{
    MELON_ASSERT(capacity <= MELON_HANDLE_INDEX_MAX);

    pool->allocator = *allocator;
    pool->handle_entries
        = (melon_handle_entry*) MELON_ALLOC(pool->allocator, sizeof(melon_handle_entry) * capacity, MELON_DEFAULT_ALIGN);
    pool->capacity = capacity;
    pool->grow_by_default = grow_by_default;

    melon_pool_reset(pool);
}

void melon_delete_handle_pool(melon_handle_pool* pool) { MELON_FREE(pool->allocator, pool->handle_entries); }

void melon_pool_reset(melon_handle_pool* pool)
{
    pool->freelist_head_index = MELON_HANDLE_INDEX_INVALID;
    pool->freelist_tail_index = MELON_HANDLE_INDEX_INVALID;
    for (size_t i = 0; i < pool->capacity; i++)
    {
        melon_handle new_handle = i;
        push_free_handle(pool, new_handle);
    }
}

melon_handle melon_pool_create_handle(melon_handle_pool* pool)
{
    // Try to pop a handle off the freelist
    melon_handle new_handle = pop_free_handle(pool);
    if (new_handle != MELON_INVALID_HANDLE)
    {
        return new_handle;
    }

    // If new_handle is invalid, it means we have to reallocate or return an invalid handle if the pool is not grow_by_default
    if (!pool->grow_by_default)
    {
        return MELON_INVALID_HANDLE;
    }

    // If the new capacity is beyond the address space of the index, do not reallocate
    size_t double_capacity = pool->capacity * 2;
    size_t new_capacity = double_capacity > MELON_HANDLE_INDEX_CAPACITY ? MELON_HANDLE_INDEX_CAPACITY : double_capacity;

    // If this part of the code is reached, then there's no more free indices and we can't expand. All there is
    // to do is to wait for a handle to be freed.
    if (new_capacity == pool->capacity)
    {
        return MELON_INVALID_HANDLE;
    }

    pool->handle_entries = (melon_handle_entry*) MELON_REALLOC(
        pool->allocator, pool->handle_entries, sizeof(melon_handle_entry) * new_capacity, MELON_DEFAULT_ALIGN);

    // Push new handles
    for (size_t i = pool->capacity; i < new_capacity; i++)
    {
        melon_handle allocated_handle = i;
        push_free_handle(pool, allocated_handle);
    }

    pool->capacity = new_capacity;

    return pop_free_handle(pool);
}

bool melon_handle_is_valid(melon_handle_pool* pool, melon_handle handle)
{
    uint64_t index      = handle_index(handle);
    uint64_t generation = handle_generation(handle);
    return handle != MELON_INVALID_HANDLE && generation < MELON_HANDLE_GENERATION_MAX
           && pool->handle_entries[index].handle == handle && index < pool->capacity;
}

melon_handle melon_pool_invalid_handle() { return MELON_INVALID_HANDLE; }

bool melon_pool_delete_handle(melon_handle_pool* pool, melon_handle handle)
{
    if (!melon_handle_is_valid(pool, handle))
    {
        return false;
    }

    // Increment the handle's generation
    handle_increment_generation(&handle);

    push_free_handle(pool, handle);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// melon_map - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

void _melon_create_map(void** data_ptr, _melon_map* pv, size_t capacity, size_t element_size,
                       const melon_allocator_api* allocator, bool grow_by_default)
{
    pv->allocator       = *allocator;
    pv->capacity        = capacity;
    pv->element_size    = element_size;
    pv->grow_by_default = grow_by_default;

    melon_create_handle_pool(&pv->pool, capacity, allocator, grow_by_default);
    pv->data    = data_ptr;
    *(pv->data) = MELON_ALLOC(pv->allocator, capacity * element_size, element_size);
}

void _melon_delete_map(_melon_map* pv)
{
    melon_delete_handle_pool(&pv->pool);
    MELON_FREE(pv->allocator, *(pv->data));
}

melon_handle _melon_map_push(_melon_map* pv, const void* val)
{
    melon_handle new_handle = melon_pool_create_handle(&pv->pool);
    if (!melon_handle_is_valid(&pv->pool, new_handle) && !(pv->grow_by_default))
    {
        return melon_pool_invalid_handle();
    }

    size_t       new_index  = handle_index(new_handle);
    if (new_index < pv->capacity)
    {
        memcpy((void*) ((uint8_t*) (*(pv->data)) + (pv->element_size * new_index)), val, pv->element_size);
        return new_handle;
    }

    size_t double_capacity = pv->capacity * 2;
    size_t new_capacity    = double_capacity > new_index ? double_capacity : new_index + 1;
    *(pv->data)            = MELON_REALLOC(pv->allocator, *(pv->data), pv->element_size * new_capacity, MELON_DEFAULT_ALIGN);
    pv->capacity           = new_capacity;

    memcpy((void*) ((uint8_t*) (*(pv->data)) + (pv->element_size * new_index)), val, pv->element_size);

    return new_handle;
}

bool _melon_map_set(_melon_map* pv, melon_handle handle, const void* val)
{
    if (!melon_handle_is_valid(&(pv->pool), handle))
    {
        return false;
    }

    memcpy((void*) ((uint8_t*) (*(pv->data)) + (pv->element_size * handle_index(handle))), val, pv->element_size);

    return true;
}

bool _melon_map_delete(_melon_map* pv, const melon_handle handle)
{
    if (!melon_handle_is_valid(&(pv->pool), handle))
    {
        return false;
    }

    melon_pool_delete_handle(&pv->pool, handle);

    return true;
}
