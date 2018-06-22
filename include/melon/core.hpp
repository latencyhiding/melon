#ifndef MELON_CORE_H
#define MELON_CORE_H

// Standard headers
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <tinycthread.h>

#include <melon/error.hpp>

namespace melon
{

/* Convenience macros for data sizes
 */

#define MELON_KILOBYTE(n) (1024 * n)
#define MELON_MEGABYTE(n) (1024 * MELON_KILOBYTE(n))
#define MELON_GIGABYTE(n) (1024 * MELON_MEGABYTE(n))

// Utility for aligning pointers
static inline void* align_forward(void* ptr, size_t align)
{
    if (align == 0)
        return ptr;
    uintptr_t uint_ptr = (uintptr_t) ptr;
    return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static inline size_t aligned_size(void* ptr, size_t size, size_t align)
{
    uint8_t* ptr_u8 = (uint8_t*) ptr;
    return (uint8_t*) align_forward(ptr_u8 + size, align) - ptr_u8;
}

// Functors for allocation callbacks
typedef struct
{
    void* (*alloc)(void* user_data, size_t size, size_t align);
    void* (*realloc)(void* user_data, void* ptr, size_t size, size_t align);
    void (*dealloc)(void* user_data, void* ptr);

    void* user_data;
} allocator;

// Convenience macros for the callbacks

#define MELON_ALLOC(allocator, size, align) (allocator.alloc(allocator.user_data, size, align))
#define MELON_REALLOC(allocator, ptr, size, align) (allocator.realloc(allocator.user_data, ptr, size, align))
#define MELON_FREE(allocator, ptr) (allocator.dealloc(allocator.user_data, ptr))

typedef void (*logger_callback_fp)(const char* message, ...);
extern logger_callback_fp logger_callback;

#ifdef MELON_DEBUG
#define MELON_LOG(...) logger_callback(__VA_ARGS__)
#else
#define MELON_LOG(...)
#endif

// Returns the default callbacks for our backend
const allocator* default_cb_allocator();

////////////////////////////////////////////////////////////////////////////////
// arena - fixed sized linear memory areas backed by custom allocators
////////////////////////////////////////////////////////////////////////////////

typedef struct memory_block
{
    uint8_t* start;
    size_t   offset;
    size_t   size;

    allocator            allocator;
    struct memory_block* prev;
} memory_block;

typedef enum
{
    MELON_NO_ALLOC_FLAGS      = 0,
    MELON_ALLOC_EXPAND_DOUBLE = 1 << 1
} alloc_flag;

typedef struct
{
    memory_block* current_block;

    uint32_t allocation_flags;
} memory_arena;

#define MELON_DEFAULT_ALIGN 16

#define MELON_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define MELON_ARENA_PUSH(arena, size, align) arena_push_size(&arena, size, align)
#define MELON_ARENA_PUSH_STRUCT(arena, T) ((T*) MELON_ARENA_PUSH(arena, sizeof(T), sizeof(T)))
#define MELON_ARENA_PUSH_ARRAY(arena, T, length, align) ((T*) MELON_ARENA_PUSH(arena, sizeof(T) * length), sizeof(T))

memory_arena create_arena(memory_block* prev, uint32_t alloc_flags, size_t size, size_t align, const allocator* alloc);
inline memory_arena create_arena(uint32_t alloc_flags, size_t size, size_t align, const allocator* alloc)
{
    return create_arena(NULL, alloc_flags, size, align, alloc);
}
inline memory_arena create_arena(size_t size, size_t align, const allocator* alloc)
{
    return create_arena(NULL, MELON_NO_ALLOC_FLAGS, size, align, alloc);
}
void  destroy_arena(memory_arena* arena);
void* arena_push_size(memory_arena* arena, size_t size, size_t align);
void  arena_reset(memory_arena* arena);

////////////////////////////////////////////////////////////////////////////////
// pool - an index pool with a stack-like behavior. Indices are allocated in
// a FIFO order.
////////////////////////////////////////////////////////////////////////////////

typedef size_t pool_index;

typedef struct
{
    pool_index* free_indices;
    size_t      capacity;
    size_t      num_free_indices;

    allocator allocator;
} index_pool;

void       create_pool(index_pool* pool, size_t capacity, const allocator* allocator);
void       delete_pool(index_pool* pool);
pool_index pool_create_index(index_pool* pool);
bool       pool_index_is_valid(index_pool* pool, pool_index id);
pool_index pool_gen_invalid_index();
bool       pool_delete_index(index_pool* pool, pool_index index);

////////////////////////////////////////////////////////////////////////////////
// pool_vector - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    index_pool pool;

    void*  data;
    size_t capacity;
    size_t element_size;

    allocator allocator;
} pool_vector;

void       create_pool_vector(pool_vector* pv, size_t capacity, size_t element_size, const allocator* allocator);
void       delete_pool_vector(pool_vector* pv);
pool_index pool_vector_push(pool_vector* pv, void* val);

#define MELON_POOL_VECTOR(name, T)                                                                                    \
    typedef struct                                                                                                    \
    {                                                                                                                 \
        pool_vector pv;                                                                                               \
    } name##_pool;                                                                                                    \
    static inline void create_##name##_pool(name##_pool* pv, size_t capacity, const allocator* allocator)             \
    {                                                                                                                 \
        create_pool_vector(&pv->pv, capacity, sizeof(T), allocator);                                                  \
    }                                                                                                                 \
    static inline void       delete_##name##_pool(name##_pool* pv) { delete_pool_vector(&pv->pv); }                   \
    static inline T          name##_pool_get(name##_pool* pv, pool_index id) { return ((T*) pv->pv.data)[id]; }       \
    static inline T*         name##_pool_get_p(name##_pool* pv, pool_index id) { return ((T*) pv->pv.data) + id; }    \
    static inline void       name##_pool_set(name##_pool* pv, pool_index id, T val) { ((T*) pv->pv.data)[id] = val; } \
    static inline pool_index name##_pool_push(name##_pool* pv, T val) { return pool_vector_push(&pv->pv, &val); }     \
    static inline bool       name##_pool_delete(name##_pool* pv, pool_index index)                                    \
    {                                                                                                                 \
        return pool_delete_index(&pv->pv.pool, index);                                                                \
    }                                                                                                                 \
    static inline bool name##_pool_index_is_valid(name##_pool* pv, pool_index index)                                  \
    {                                                                                                                 \
        return pool_index_is_valid(&pv->pv.pool, index);                                                              \
    }

}    // namespace melon

#endif
