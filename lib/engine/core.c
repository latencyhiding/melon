#include "core.h"

////////////////////////////////////////////////////////////////////////////////
// Default memory allocation callbacks
////////////////////////////////////////////////////////////////////////////////

static void* tz_malloc(void* user_data, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* ptr = malloc(size + offset);

  if (ptr == NULL)
    return NULL;

  void** return_ptr = (void**) align_forward(((void**) ptr) + 1, align);
  return_ptr[-1] = ptr;

  return (void*) return_ptr;
}

static void* tz_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* new_ptr = realloc(ptr, size + offset);

  if (new_ptr == NULL)
    return NULL;

  void** return_ptr = (void**) align_forward(((void**) new_ptr) + 1, align);
  return_ptr[-1] = new_ptr;

  return (void*) return_ptr;
}

static void tz_free(void* user_data, void* ptr)
{
  free(((void**) ptr)[-1]);
}

const tz_cb_allocator* tz_default_cb_allocator()
{
  static tz_cb_allocator* p_allocator = NULL;
  static tz_cb_allocator allocator;

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
// Default logging callbacks
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

static void tz_default_logger(const char* message, ...)
{
  va_list arg_list;
  va_start(arg_list, message);
  vfprintf(stderr,  message, arg_list);
  va_end(arg_list);
}

tz_cb_logger tz_logger_callback = tz_default_logger;

////////////////////////////////////////////////////////////////////////////////
// Pool implementation 
////////////////////////////////////////////////////////////////////////////////

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator)
{
  pool->allocator = *allocator;
  pool->free_indices = (tz_pool_index*) TZ_ALLOC(pool->allocator, sizeof(tz_pool_index) * capacity, 4);
  pool->capacity = capacity;
  pool->num_free_indices = capacity;

  for (tz_pool_index i = 0; i < capacity; i++)
    pool->free_indices[i] = i;
}

void tz_delete_pool(tz_pool* pool)
{
  TZ_FREE(pool->allocator, pool->free_indices);
}

tz_pool_index tz_pool_create_id(tz_pool* pool)
{
  if (pool->num_free_indices == 0)
    return TZ_POOL_INVALID_INDEX;

  return pool->free_indices[--pool->num_free_indices];
}

void tz_pool_delete_id(tz_pool* pool, tz_pool_index index)
{
  if (index == TZ_POOL_INVALID_INDEX)
    return;

  if (pool->num_free_indices >= pool->capacity)
    return;

  pool->free_indices[pool->num_free_indices++] = index;
}

