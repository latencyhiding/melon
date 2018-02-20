#ifndef TZ_ERROR_H
#define TZ_ERROR_H

#include <stdlib.h>

#define TZ_ASSERT(cond, ...) do { if (!cond) { fprintf(stderr, "ASSERT FAILURE: \"" #cond "\" "); fprintf(stderr, __VA_ARGS__ "\n"); exit(EXIT_FAILURE);} } while(0)
#define TZ_STATIC_ASSERT(cond, msg) typedef int _static_assert_##msg[(cond) ? 1 : -1];

#endif
