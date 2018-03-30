#include "file_utils.h"

const char* tz_load_text_file(const tz_cb_allocator* allocator, const char* filename)
{
  FILE* f = fopen(filename, "r");
  size_t size = 0;
  char* data = NULL;
  const tz_cb_allocator* alloc = allocator ? allocator : tz_default_cb_allocator();

  if (!f)
  {
    TZ_LOG("Can't find file: %s\n", filename);
    return NULL;
  }

  fseek(f, 0L, SEEK_END);
  size = ftell(f);
  rewind(f);
 
  data = (char*)TZ_ALLOC((*alloc), size + 1, 0);

  fread(data, 1, size, f);
  fclose(f);

  data[size] = 0;

  return data;
}