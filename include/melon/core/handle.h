#ifndef MELON_HANDLE_VECTOR_H
#define MELON_HANDLE_VECTOR_H

#include <melon/core/memory.h>
#include <stdbool.h>

////////////////////////////////////////////////////////////////////////////////
// pool - an index pool with a stack-like behavior. Indices are allocated in
// a FIFO order.
////////////////////////////////////////////////////////////////////////////////

/* TODO:
 * Additional "data" member of handle_entry?
 * - PROs:
 *      - Optional data lets user index things other than flat arrays
 *      - Less indirection when doing a lookup of above, because user won't
 *        have to create their own array to map indices to pointers/offsets
 * - CONs:
 *      - handle_entries array slightly less packed, possible cache issues
 */

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint64_t melon_handle;
#define MELON_INVALID_HANDLE (~(melon_handle) (0))

#define MELON_HANDLE_INDEX_BITS 32
#define MELON_HANDLE_INDEX_MASK (((melon_handle) 1 << MELON_HANDLE_INDEX_BITS) - 1)
#define MELON_HANDLE_INDEX_MAX (MELON_HANDLE_INDEX_MASK - 1)
#define MELON_HANDLE_INDEX_CAPACITY MELON_HANDLE_INDEX_MASK
#define MELON_HANDLE_INDEX_INVALID MELON_HANDLE_INDEX_MASK

#define MELON_HANDLE_GENERATION_BITS 32
#define MELON_HANDLE_GENERATION_MAX (((melon_handle) 1 << MELON_HANDLE_GENERATION_BITS) - 1)
#define MELON_HANDLE_GENERATION_MASK (MELON_HANDLE_GENERATION_MAX << MELON_HANDLE_INDEX_BITS)

static inline uint64_t melon_handle_index(melon_handle handle) { return handle & MELON_HANDLE_INDEX_MASK; }

typedef struct
{
    melon_handle handle;
    // size_t data_offset;
    size_t next_handle_index;
} melon_handle_entry;

typedef struct
{
    melon_handle_entry* handle_entries;
    size_t            capacity;

    size_t freelist_head_index;
    size_t freelist_tail_index;

    bool grow_by_default;

    melon_allocator_api allocator;
} melon_handle_pool;

void melon_create_handle_pool(melon_handle_pool* pool, size_t capacity, const melon_allocator_api* allocator, bool grow_by_default);
void melon_delete_handle_pool(melon_handle_pool* pool);
void melon_pool_reset(melon_handle_pool* pool);

// Create a new handle. The grow parameter overrides the "grow_by_default" flag set on creation
melon_handle melon_pool_create_handle(melon_handle_pool* pool);
bool         melon_handle_is_valid(melon_handle_pool* pool, melon_handle handle);
melon_handle melon_pool_invalid_handle();
bool         melon_pool_delete_handle(melon_handle_pool* pool, melon_handle index);

////////////////////////////////////////////////////////////////////////////////
// melon_map - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

#define MELON_HANDLE_MAP_TYPEDEF(T) \
    typedef struct                     \
    {                                  \
        T*         data;               \
        _melon_map map;                \
    } melon_map_##T;

#define melon_create_map(vec, capacity, allocator, grow_by_default) \
    _melon_create_map((void**) &((vec)->data), &((vec)->map), capacity, sizeof(*((vec)->data)), allocator, grow_by_default)
#define melon_delete_map(vec) _melon_delete_map(&((vec)->map))
#define melon_map_push(vec, val) _melon_map_push(&((vec)->map), (void*) (val))
#define melon_map_get(vec, handle) \
    (melon_handle_is_valid(&((vec)->map.pool), handle) ? (vec)->data + melon_handle_index(handle) : NULL)
#define melon_map_set(vec, handle, val) _melon_map_set(&((vec)->map), handle, (const* void) (val))
#define melon_map_delete(vec, handle) _melon_map_delete(&((vec)->map), handle)
#define melon_map_handle_is_valid(vec, handle) melon_handle_is_valid(&(((vec)->map).pool), handle)

typedef struct
{
    melon_handle_pool pool;

    void** data;
    size_t capacity;
    size_t element_size;
    bool   grow_by_default;

    melon_allocator_api allocator;
} _melon_map;

// Growable indicates grow_by_default by default
void _melon_create_map(void** data_ptr, _melon_map* pv, size_t capacity, size_t element_size,
                       const melon_allocator_api* allocator, bool grow_by_default);
void _melon_delete_map(_melon_map* pv);
// Push a new object with a new handle. The grow parameter overrides the "grow_by_default" flag set on creation
melon_handle _melon_map_push(_melon_map* pv, const void* val);
bool         _melon_map_delete(_melon_map* pv, const melon_handle handle);
bool         _melon_map_set(_melon_map* pv, melon_handle handle, const void* val);

#ifdef __cplusplus
}
#endif
#endif