#ifndef TZ_ERROR_H
#define TZ_ERROR_H

#include <stdlib.h>
#include <stdio.h>

#define TZ_ASSERT(cond, ...) do { if (!(cond)) { fprintf(stderr, "ASSERT FAILURE: \"" #cond "\" at line %d in %s. ", __LINE__, __FILE__); fprintf(stderr, "MESSAGE: " __VA_ARGS__ ); exit(EXIT_FAILURE);} } while(0)
#define TZ_STATIC_ASSERT(cond, msg) typedef int _static_assert_##msg[(cond) ? 1 : -1];

#endif
