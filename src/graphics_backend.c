#include "graphics_backend.h"

#include <stdint.h>
#include <stdlib.h>

inline static void* align_forward(void* ptr, size_t align)
{
  uintptr_t uint_ptr = (uintptr_t) ptr;
  return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static void* tz_malloc(void* user_data, size_t size, size_t align)
{
  void* ptr = malloc(size + align);

  return ptr ? (void*) align_forward(ptr, align) : NULL;
}

static void* tz_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
  void* new_ptr = realloc(ptr, size + align);

  return new_ptr ? (void*) align_forward(new_ptr, align) : NULL;
}

static void tz_free(void* user_data, void* ptr)
{
  free(ptr);
}

tz_cb_allocator tz_default_cb_allocator()
{
  tz_cb_allocator allocator;
  allocator.alloc = tz_malloc;
  allocator.realloc = tz_realloc;
  allocator.dealloc = tz_free;
  allocator.user_data = NULL;

  return allocator;
}

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  GLuint* shader_stages;
  GLuint* shader_programs;
  GLuint* buffers;
  tz_vertex_format* vertex_formats; 
  tz_pipeline* pipelines;

} tz_gfx_device_gl;

tz_gfx_device* tz_create_device(const tz_gfx_device_config* device_config)
{
  size_t total_size = sizeof(tz_gfx_device_gl); //  
}
