#include "tz_core.h"

////////////////////////////////////////////////////////////////////////////////
// Default memory allocation callbacks
////////////////////////////////////////////////////////////////////////////////

static void* tz_malloc(void* user_data, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* ptr = malloc(size + offset);

  if (ptr == NULL)
    return NULL;

  void** return_ptr = (void**) tz_align_forward(((void**) ptr) + 1, align);
  return_ptr[-1] = ptr;

  return (void*) return_ptr;
}

static void* tz_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* new_ptr = realloc(ptr, size + offset);

  if (new_ptr == NULL)
    return NULL;

  void** return_ptr = (void**) tz_align_forward(((void**) new_ptr) + 1, align);
  return_ptr[-1] = new_ptr;

  return (void*) return_ptr;
}

static void tz_free(void* user_data, void* ptr)
{
  free(((void**) ptr)[-1]);
}

const tz_allocator* tz_default_cb_allocator()
{
  static tz_allocator* p_allocator = NULL;
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
// Block pool implementation
////////////////////////////////////////////////////////////////////////////////

#define TZ_INVALID_BLOCK_INDEX (~0)

/*
 * Fixed size blocks are allocated out of large preallocated arenas
 * There is a list of blocks and their positions in the arena
 * There is a freelist that tracks the memory blocks that can be claimed
 * 
 * Memory allocated to the pool is accessed with a "tag" which represents
 * a chain of blocks that must be freed at the same time.
 * 
 * Blocks are fixed size, but can point to other blocks when one has run
 * out of memory.
 */

void tz_create_block_pool(tz_block_pool*         block_pool,
                          size_t                 block_size,
                          size_t                 num_blocks,
                          const tz_allocator* allocator)
{
  block_pool->allocator = *allocator;

  /**
   * Block arena format:
   * 
   * | block data | next block pointer | blocks | 
   * 
   */

}

void tz_delete_block_pool(tz_block_pool* block_pool)
{
}

tz_block_pool_tag tz_block_pool_new_arena(tz_block_pool* block_pool)
{
}

void* tz_block_pool_push_arena(tz_block_pool*    block_pool,
                               tz_block_pool_tag tag,
                               size_t            size,
                               size_t            align)
{
}

void tz_block_pool_free_arena(tz_block_pool*    block_pool,
                              tz_block_pool_tag tag)
{
}

void tz_block_pool_free_all(tz_block_pool* block_pool)
{
}

////////////////////////////////////////////////////////////////////////////////
// Pool implementation 
////////////////////////////////////////////////////////////////////////////////

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_allocator* allocator)
{
  pool->allocator = *allocator;
  pool->free_indices = (uint32_t*)TZ_ALLOC(pool->allocator, sizeof(uint32_t) * capacity, 4);
  pool->generations = (uint8_t*)TZ_ALLOC(pool->allocator, sizeof(uint8_t) * capacity, 4);
  pool->capacity = capacity;
  pool->num_free_indices = capacity;

  for (uint32_t i = 0; i < capacity; i++)
    pool->free_indices[capacity - 1 - i] = i;

  for (uint8_t i = 0; i < capacity; i++)
    pool->generations[i] = 0;
}

void tz_delete_pool(tz_pool* pool)
{
  TZ_FREE(pool->allocator, pool->free_indices);
  TZ_FREE(pool->allocator, pool->generations);
}

tz_pool_id tz_pool_create_id(tz_pool* pool)
{
  tz_pool_id new_id = tz_pool_gen_invalid_id();

  uint32_t index;
  uint8_t generation;

  // Pop free indices off the stack until one with a valid generation is found
  do
  {
    if (pool->num_free_indices == 0)
      return new_id;

    index = pool->free_indices[--pool->num_free_indices];
    generation = pool->generations[index];
  } while (generation >= TZ_POOL_MAX_GENERATION);

  new_id._initialized = true;
  new_id.index = index;
  new_id.generation = generation;

  return new_id;
}

bool tz_pool_id_is_valid(tz_pool* pool, tz_pool_id id)
{
  return id._initialized
    && (id.generation == pool->generations[id.index])
    && (id.index < pool->capacity);
}

tz_pool_id tz_pool_gen_invalid_id()
{
  return (tz_pool_id) { 0 };
}

bool tz_pool_delete_id(tz_pool* pool, tz_pool_id id)
{
  if (!tz_pool_id_is_valid(pool, id))
    return false;

  // if the number of free indices >= the capacity, the pool is empty
  if (pool->num_free_indices >= pool->capacity)
    return false;

  pool->free_indices[pool->num_free_indices++] = id.index;
  pool->generations[id.index]++;

  return true;
}

