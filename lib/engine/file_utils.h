#ifndef TZ_FILE_UTILS_H
#define TZ_FILE_UTILS_H

#include <stdio.h>
#include "core.h"

const char* tz_load_text_file(const tz_cb_allocator* allocator, const char* filename);

#endif