#ifndef TZ_CORE_H 
#define TZ_CORE_H

// Standard headers
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include <tinycthread.h>

// Utility for aligning pointers
inline static void* tz_align_forward(void* ptr, size_t align)
{
  if (align == 0)
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
#define TZ_LOG(...) tz_logger_callback(__VA_ARGS__)
#else
#define TZ_LOG(...)
#endif

// Returns the default callbacks for our backend
const tz_cb_allocator* tz_default_cb_allocator();

////////////////////////////////////////////////////////////////////////////////
// tz_block_pool - a thread safe, block-based memory pool
////////////////////////////////////////////////////////////////////////////////

typedef struct tz_arena
{
  uint8_t* start;
  size_t offset;
  size_t size;

  struct tz_arena* next;
} tz_arena;

#define TZ_ALLOC_ARENA(alloc, size, align) ((tz_arena) { TZ_ALLOC(alloc, size, align), 0, size, NULL})
#define TZ_FREE_ARENA(alloc, arena) (TZ_FREE(alloc, arena.start))

typedef struct tz_block
{
  uint8_t* start;
  size_t used;
  struct tz_block* next;

  // If freed, this will store the next block in the freelist
  struct tz_block* freelist_next;
} tz_block;

typedef struct
{
  tz_arena memory;

  tz_block* freelist_head;
  tz_block* freelist_tail;

  size_t block_size;
  size_t num_blocks;

  tz_cb_allocator allocator;

  cnd_t growth_cnd;
  mtx_t growth_mtx;

  mtx_t freelist_mtx;
} tz_block_pool;

void tz_create_block_pool(tz_block_pool*         block_pool,
                          size_t                 block_size,
                          size_t                 num_blocks,
                          const tz_cb_allocator* allocator);
void tz_delete_block_pool(tz_block_pool* block_pool);
size_t tz_block_pool_new_chain(tz_block_pool* block_pool);
void* tz_block_pool_push_chain(tz_block_pool* block_pool,
                               size_t         chain_id,
                               size_t         size,
                               size_t         align);
void tz_block_pool_free_chain(tz_block_pool* block_pool,
                              size_t         chain_id);
void tz_block_pool_free_all(tz_block_pool* block_pool);

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
  uint32_t* free_indices;
  uint8_t* generations;
  size_t capacity;
  size_t num_free_indices;

  tz_cb_allocator allocator;
} tz_pool;

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator);
void tz_delete_pool(tz_pool* pool);
tz_pool_id tz_pool_create_id(tz_pool* pool);
bool tz_pool_id_is_valid(tz_pool* pool, tz_pool_id id);
tz_pool_id tz_pool_gen_invalid_id();
bool tz_pool_delete_id(tz_pool* pool, tz_pool_id index);

#define TZ_ID(name) typedef union { tz_pool_id id; uint32_t data; } name; \
                    static inline name name##_id_init(tz_pool* pool) \
                    {\
                      return (name) { tz_pool_create_id(pool) };\
                    }\
                    static inline bool name##_id_delete(tz_pool* pool, name id) { return tz_pool_delete_id(pool, id.id); } \
                    static inline bool name##_id_is_valid(tz_pool* pool, name id) { return tz_pool_id_is_valid(pool, id.id); }
#define TZ_INVALID_ID(type) (type) { tz_pool_gen_invalid_id() };
#define TZ_POOL_MAX_GENERATION ((uint8_t) ~0)

/* Convenience macros for data sizes
 */

#define TZ_KILOBYTE(n) (1024 * n)
#define TZ_MEGABYTE(n) (1024 * TZ_KILOBYTE(n))
#define TZ_GIGABYTE(n) (1024 * TZ_MEGABYTE(n))

#endif
