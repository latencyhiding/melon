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
// Arena functions
////////////////////////////////////////////////////////////////////////////////

tz_arena tz_create_arena(tz_memory_block *prev, uint32_t alloc_flags, size_t size, size_t align, const tz_allocator *alloc)
{
  tz_memory_block* result = alloc->alloc(alloc->user_data, size + align + sizeof(tz_memory_block), align);
  result->start = (uint8_t*) tz_align_forward(result + 1, align);
  result->offset = 0;
  result->size = size;
  result->allocator = *alloc;
  result->prev = prev;

  tz_arena arena;
  arena.current_block = result;

  return arena;
}

void tz_destroy_arena(tz_arena* arena)
{
  while (arena->current_block)
  {
    tz_memory_block* prev = arena->current_block->prev;
    TZ_FREE(arena->current_block->allocator, arena->current_block);

    arena->current_block = prev;
  }
}

void* tz_arena_push_size(tz_arena* arena, size_t size, size_t align)
{
  // Try to increment offset on current block
  tz_memory_block* block = arena->current_block;
  uint8_t* result = (uint8_t*) tz_align_forward(block->start + block->offset, align);
  size_t offset = result - block->start + size;

  // If the new offset is within the block, return the new pointer
  if (offset < block->size)
  {
    block->offset = offset;
    return (void*) result;
  }

  // Allocate a new block
  size_t new_block_size = arena->allocation_flags & TZ_ALLOC_EXPAND_DOUBLE ? block->size * 2 : block->size;
  while (size > new_block_size)
    new_block_size *= 2;
  new_block_size += align;

  *arena = tz_create_arena(block, arena->allocation_flags, new_block_size, align, &block->allocator);
  tz_memory_block* new_block = arena->current_block;

  result = (uint8_t*) tz_align_forward(new_block->start, align);
  new_block->offset = tz_aligned_size(new_block->start, new_block->offset + size, align);

  return result;
}

// Deallocate extra blocks, reset offset to 0
void tz_arena_reset(tz_arena* arena)
{
  tz_memory_block* current_block = arena->current_block;
  while (current_block->prev)
  {
    TZ_FREE(current_block->allocator, current_block);
  }
  current_block->offset = 0;
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

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_allocator* allocator)
{
  pool->allocator = *allocator;
  pool->free_indices = (uint32_t*)TZ_ALLOC(pool->allocator, sizeof(uint32_t) * capacity, TZ_DEFAULT_ALIGN);
  pool->generations = (uint8_t*)TZ_ALLOC(pool->allocator, sizeof(uint8_t) * capacity, TZ_DEFAULT_ALIGN);
  pool->capacity = capacity;
  pool->num_free_indices = capacity;

  for (size_t i = 0; i < capacity; i++)
    pool->free_indices[capacity - 1 - i] = i;

  for (size_t i = 0; i < capacity; i++)
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
    {
      // Reallocate
      size_t new_capacity = pool->capacity * 2;
      pool->free_indices = TZ_REALLOC(pool->allocator, pool->free_indices, new_capacity, TZ_DEFAULT_ALIGN);
      pool->generations = TZ_REALLOC(pool->allocator, pool->generations, new_capacity, TZ_DEFAULT_ALIGN);

      for (size_t i = 0; i < new_capacity; i++)
        pool->free_indices[i] = new_capacity - 1 - i; 

      for (size_t i = pool->capacity; i < new_capacity; i++)
        pool->generations[i] = 0; 

      pool->capacity = new_capacity;
      pool->num_free_indices = new_capacity;
    }

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

