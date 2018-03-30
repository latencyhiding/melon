#ifndef TZ_CORE_H 
#define TZ_CORE_H

// Standard headers
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

// Utility for aligning pointers
inline static void* align_forward(void* ptr, size_t align)
{
  if (!align)
    return ptr;
  uintptr_t uint_ptr = (uintptr_t) ptr;
  return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

// Functors for allocation callbacks
typedef struct
{
  void* (*alloc) (void* user_data, size_t size, size_t align);
  void* (*realloc) (void* user_data, void* ptr, size_t size, size_t align);
  void (*dealloc) (void* user_data, void* ptr);

  void* user_data;
} tz_cb_allocator;

// Convenience macros for the callbacks

#define TZ_ALLOC(allocator, size, align) (allocator.alloc(allocator.user_data, size, align))
#define TZ_REALLOC(allocator, ptr, size, align) (allocator.realloc(allocator.user_data, ptr, size, align))
#define TZ_FREE(allocator, ptr) (allocator.dealloc(allocator.user_data, ptr))

typedef void(*tz_cb_logger) (const char* message, ...);
extern tz_cb_logger tz_logger_callback;

#ifdef TZ_DEBUG
#define TZ_LOG(message, ...) tz_logger_callback(message, __VA_ARGS__)
#else
#define TZ_LOG(message, ...)
#endif

// Returns the default callbacks for our backend
const tz_cb_allocator* tz_default_cb_allocator();

/* tz_pool - a pool with a stack-like behavior. Indices are allocated in a FIFO
 * order.
 */
#define TZ_POOL_INVALID_INDEX ~0
#define TZ_IS_INVALID_ID(id) (id.index == TZ_POOL_INVALID_INDEX)
typedef uint32_t tz_pool_index;
#define TZ_ID(name) typedef struct {tz_pool_index index;} name;

typedef struct
{
  tz_pool_index* free_indices;
  size_t capacity;
  size_t num_free_indices;

  tz_cb_allocator allocator;
} tz_pool;

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator);
void tz_delete_pool(tz_pool* pool);
tz_pool_index tz_pool_create_id(tz_pool* pool);
void tz_pool_delete_id(tz_pool* pool, tz_pool_index index);

#endif