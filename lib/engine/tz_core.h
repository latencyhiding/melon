#ifndef TZ_CORE_H 
#define TZ_CORE_H

// Standard headers
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include <tinycthread.h>

// Utility for aligning pointers
static inline void* tz_align_forward(void* ptr, size_t align)
{
  if (align == 0)
    return ptr;
  uintptr_t uint_ptr = (uintptr_t) ptr;
  return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static inline size_t tz_aligned_size(void* ptr, size_t size, size_t align)
{
  uint8_t* ptr_u8 = (uint8_t*) ptr;
  return (uint8_t*) tz_align_forward(ptr_u8 + size, align) - ptr_u8;
}

// Functors for allocation callbacks
typedef struct
{
  void* (*alloc) (void* user_data, size_t size, size_t align);
  void* (*realloc) (void* user_data, void* ptr, size_t size, size_t align);
  void (*dealloc) (void* user_data, void* ptr);

  void* user_data;
} tz_allocator;

// Convenience macros for the callbacks

#define TZ_ALLOC(allocator, size, align) (allocator.alloc(allocator.user_data, size, align))
#define TZ_REALLOC(allocator, ptr, size, align) (allocator.realloc(allocator.user_data, ptr, size, align))
#define TZ_FREE(allocator, ptr) (allocator.dealloc(allocator.user_data, ptr))

typedef void(*tz_cb_logger) (const char* message, ...);
extern tz_cb_logger tz_logger_callback;

#ifdef TZ_DEBUG
#define TZ_LOG(...) tz_logger_callback(__VA_ARGS__)
#else
#define TZ_LOG(...)
#endif

// Returns the default callbacks for our backend
const tz_allocator* tz_default_cb_allocator();

////////////////////////////////////////////////////////////////////////////////
// tz_arena - fixed sized linear memory areas backed by custom allocators
////////////////////////////////////////////////////////////////////////////////

typedef struct tz_memory_block
{
  uint8_t *start;
  size_t offset;
  size_t size;

  tz_allocator allocator;
  struct tz_memory_block* prev;
} tz_memory_block;

typedef enum 
{
  TZ_NO_ALLOC_FLAGS = 0,
  TZ_ALLOC_EXPAND_DOUBLE = 1 << 1
} tz_alloc_flag;

typedef struct
{
  tz_memory_block *current_block;

  uint32_t allocation_flags;
} tz_arena;

#define TZ_DEFAULT_ALIGN 16

#define TZ_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME

#define TZ_ALLOC_ARENA4(alloc, size, align, alloc_flags) tz_create_arena(NULL, alloc_flags, size, align, &alloc)
#define TZ_ALLOC_ARENA3(alloc, size, align) tz_create_arena(NULL, TZ_NO_ALLOC_FLAGS, size, align, &alloc)
#define TZ_ALLOC_ARENA(...) TZ_GET_MACRO(__VA_ARGS__, TZ_ALLOC_ARENA4, TZ_ALLOC_ARENA3) \
(__VA_ARGS__)
#define TZ_FREE_ARENA(arena) tz_destroy_arena(&arena)
#define TZ_ARENA_PUSH(arena, size, align) tz_arena_push_size(&arena, size, align)
#define TZ_ARENA_PUSH_STRUCT(arena, T) ((T *)TZ_ARENA_PUSH(arena, sizeof(T), sizeof(T)))
#define TZ_ARENA_PUSH_ARRAY(arena, T, capacity, align) ((T *)TZ_ARENA_PUSH(arena, sizeof(T) * capacity), sizeof(T))

tz_arena tz_create_arena(tz_memory_block *prev, uint32_t alloc_flags, size_t size, size_t align, const tz_allocator *alloc);
void tz_destroy_arena(tz_arena *arena);
void *tz_arena_push_size(tz_arena *arena, size_t size, size_t align);
void tz_arena_reset(tz_arena *arena);

////////////////////////////////////////////////////////////////////////////////
// tz_pool - an index pool with a stack-like behavior. Indices are allocated in
// a FIFO order.
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  bool _initialized : 1;
  uint8_t generation : 7;
  uint32_t index : 24;

} tz_pool_id;

typedef struct
{
  uint32_t *free_indices;
  uint8_t *generations;
  size_t capacity;
  size_t num_free_indices;

  tz_allocator allocator;
} tz_pool;

void tz_create_pool(tz_pool *pool, size_t capacity, const tz_allocator *allocator);
void tz_delete_pool(tz_pool *pool);
tz_pool_id tz_pool_create_id(tz_pool *pool);
bool tz_pool_id_is_valid(tz_pool *pool, tz_pool_id id);
tz_pool_id tz_pool_gen_invalid_id();
bool tz_pool_delete_id(tz_pool *pool, tz_pool_id index);

#define TZ_ID(name)                                                                                      \
  typedef union {                                                                                        \
    tz_pool_id id;                                                                                       \
    uint32_t data;                                                                                       \
  } name;                                                                                                \
  static inline name name##_id_create(tz_pool *pool)                                                     \
  {                                                                                                      \
    return (name){tz_pool_create_id(pool)};                                                              \
  }                                                                                                      \
  static inline bool name##_id_delete(tz_pool *pool, name id) { return tz_pool_delete_id(pool, id.id); } \
  static inline bool name##_id_is_valid(tz_pool *pool, name id) { return tz_pool_id_is_valid(pool, id.id); }
#define TZ_INVALID_ID(type) (type){tz_pool_gen_invalid_id()};
#define TZ_POOL_MAX_GENERATION ((uint8_t)~0)
#define TZ_POOL_MAX_INDEX ((uint32_t)~0)

/* Convenience macros for data sizes
 */

#define TZ_KILOBYTE(n) (1024 * n)
#define TZ_MEGABYTE(n) (1024 * TZ_KILOBYTE(n))
#define TZ_GIGABYTE(n) (1024 * TZ_MEGABYTE(n))

#endif
