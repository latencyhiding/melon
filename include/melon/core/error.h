#ifndef MELON_ERROR_H
#define MELON_ERROR_H

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*melon_logger_callback_fp)(const char* message, ...);
extern melon_logger_callback_fp melon_logger_callback;

#ifdef MELON_DEBUG
#define MELON_LOG(...) melon_logger_callback(__VA_ARGS__)
#define MELON_ASSERT(cond, ...) do { if (!(cond)) { fprintf(stderr, "ASSERT FAILURE: \"" #cond "\" at line %d in %s. ", __LINE__, __FILE__); fprintf(stderr, "MESSAGE: " __VA_ARGS__ ); exit(0);} } while(0)
#else
#define MELON_LOG(...)
#define MELON_ASSERT(cond, ...)
#endif

#define MELON_STATIC_ASSERT(cond, msg) typedef int _static_assert_##msg[(cond) ? 1 : -1];

#ifdef __cplusplus
}
#endif

#endif
