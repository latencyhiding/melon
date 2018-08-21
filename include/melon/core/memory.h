#ifndef MELON_MEMORY_H
#define MELON_MEMORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Convenience macros for data sizes
 */

#define MELON_KILOBYTE(n) (1024 * n)
#define MELON_MEGABYTE(n) (1024 * MELON_KILOBYTE(n))
#define MELON_GIGABYTE(n) (1024 * MELON_MEGABYTE(n))

// Utility for aligning pointers
static inline void* melon_align_forward(void* ptr, size_t align)
{
    if (align == 0)
        return ptr;
    uintptr_t uint_ptr = (uintptr_t) ptr;
    return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static inline size_t melon_aligned_size(void* ptr, size_t size, size_t align)
{
    uint8_t* ptr_u8 = (uint8_t*) ptr;
    return (uint8_t*) melon_align_forward(ptr_u8 + size, align) - ptr_u8;
}

// Functors for allocation callbacks
typedef struct
{
    void* (*alloc)(void* user_data, size_t size, size_t align);
    void* (*realloc)(void* user_data, void* ptr, size_t size, size_t align);
    void (*dealloc)(void* user_data, void* ptr);

    void* user_data;
} melon_allocator_api;

// Convenience macros for the callbacks

#define MELON_ALLOC(allocator, size, align) (allocator.alloc(allocator.user_data, size, align))
#define MELON_REALLOC(allocator, ptr, size, align) (allocator.realloc(allocator.user_data, ptr, size, align))
#define MELON_FREE(allocator, ptr) (allocator.dealloc(allocator.user_data, ptr))

// Returns the default callbacks for our backend
const melon_allocator_api* melon_default_cb_allocator();

////////////////////////////////////////////////////////////////////////////////
// arena - fixed sized linear memory areas backed by custom allocators
////////////////////////////////////////////////////////////////////////////////

typedef struct melon_memory_block
{
    uint8_t* start;
    size_t   offset;
    size_t   size;

    melon_allocator_api        allocator;
    struct melon_memory_block* prev;
} melon_memory_block;

typedef enum
{
    MELON_NO_ALLOC_FLAGS      = 0,
    MELON_ALLOC_EXPAND_DOUBLE = 1 << 1
} melon_alloc_flag;

typedef struct
{
    melon_memory_block* current_block;

    uint32_t allocation_flags;
} melon_memory_arena;

#define MELON_DEFAULT_ALIGN 16

#define MELON_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define MELON_ARENA_PUSH(arena, size, align) melon_arena_push_size(&arena, size, align)
#define MELON_ARENA_PUSH_STRUCT(arena, T) ((T*) MELON_ARENA_PUSH(arena, sizeof(T), sizeof(T)))
#define MELON_ARENA_PUSH_ARRAY(arena, T, length, align) ((T*) MELON_ARENA_PUSH(arena, sizeof(T) * length), sizeof(T))

melon_memory_arena melon_create_arena_appended(melon_memory_block* prev, uint32_t alloc_flags, size_t size, size_t align, const melon_allocator_api* alloc);
static inline melon_memory_arena melon_create_arena_with_options(uint32_t alloc_flags, size_t size, size_t align, const melon_allocator_api* alloc)
{
    return melon_create_arena_appended(NULL, alloc_flags, size, align, alloc);
}
static inline melon_memory_arena melon_create_arena(size_t size, size_t align, const melon_allocator_api* alloc)
{
    return melon_create_arena_appended(NULL, MELON_ALLOC_EXPAND_DOUBLE, size, align, alloc);
}
void  melon_destroy_arena(melon_memory_arena* arena);
void* melon_arena_push_size(melon_memory_arena* arena, size_t size, size_t align);
void  melon_arena_reset(melon_memory_arena* arena);

#ifdef __cplusplus
}
#endif

#endif