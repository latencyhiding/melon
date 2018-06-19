#ifndef TZ_CORE_H 
#define TZ_CORE_H

// Standard headers
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include <tinycthread.h>

#include "tz_error.h"

/* Convenience macros for data sizes
 */

#define TZ_KILOBYTE(n) (1024 * n)
#define TZ_MEGABYTE(n) (1024 * TZ_KILOBYTE(n))
#define TZ_GIGABYTE(n) (1024 * TZ_MEGABYTE(n))

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
#define TZ_ARENA_PUSH_ARRAY(arena, T, length, align) ((T *)TZ_ARENA_PUSH(arena, sizeof(T) * length), sizeof(T))

tz_arena tz_create_arena(tz_memory_block *prev, uint32_t alloc_flags, size_t size, size_t align, const tz_allocator *alloc);
void tz_destroy_arena(tz_arena *arena);
void *tz_arena_push_size(tz_arena *arena, size_t size, size_t align);
void tz_arena_reset(tz_arena *arena);

////////////////////////////////////////////////////////////////////////////////
// tz_pool - an index pool with a stack-like behavior. Indices are allocated in
// a FIFO order.
////////////////////////////////////////////////////////////////////////////////

typedef size_t tz_pool_index;

typedef struct
{
  size_t* free_indices;
  size_t capacity;
  size_t num_free_indices;

  tz_allocator allocator;
} tz_pool;

void tz_create_pool(tz_pool *pool, size_t capacity, const tz_allocator *allocator);
void tz_delete_pool(tz_pool *pool);
tz_pool_index tz_pool_create_index(tz_pool *pool);
bool tz_pool_index_is_valid(tz_pool *pool, tz_pool_index id);
tz_pool_index tz_pool_gen_invalid_index();
bool tz_pool_delete_index(tz_pool *pool, tz_pool_index index);

////////////////////////////////////////////////////////////////////////////////
// tz_pool_vector - a vector that uses an id pool for access
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  tz_pool pool;

  void* data;
  size_t capacity;
  size_t element_size;

  tz_allocator allocator;
} tz_pool_vector;

void tz_create_pool_vector(tz_pool_vector* pv, size_t capacity, size_t element_size, const tz_allocator* allocator);
void tz_delete_pool_vector(tz_pool_vector* pv);
tz_pool_index tz_pool_vector_push(tz_pool_vector* pv, void* val);

#define TZ_POOL_VECTOR(name, T) \
typedef struct \
{\
  tz_pool_vector pv;\
} tz_##name##_pool;\
static inline void tz_create_##name##_pool(tz_##name##_pool* pv, size_t capacity, const tz_allocator* allocator) { tz_create_pool_vector(&pv->pv, capacity, sizeof(T), allocator); } \
static inline void tz_delete_##name##_pool(tz_##name##_pool* pv) { tz_delete_pool_vector(&pv->pv); } \
static inline T tz_##name##_pool_get(tz_##name##_pool* pv, tz_pool_index id) { return ((T*) pv->pv.data)[id];} \
static inline T* tz_##name##_pool_get_p(tz_##name##_pool* pv, tz_pool_index id) { return ((T*) pv->pv.data) + id;} \
static inline void tz_##name##_pool_set(tz_##name##_pool* pv, tz_pool_index id, T val) { ((T*) pv->pv.data)[id] = val; } \
static inline tz_pool_index tz_##name##_pool_push(tz_##name##_pool* pv, T val) { return tz_pool_vector_push(&pv->pv, &val);} \
static inline bool tz_##name##_pool_delete(tz_##name##_pool* pv, tz_pool_index index) { return tz_pool_delete_index(&pv->pv.pool, index); } \
static inline bool tz_##name##_pool_index_is_valid(tz_##name##_pool* pv, tz_pool_index index) { return tz_pool_index_is_valid(&pv->pv.pool, index); }

#endif
