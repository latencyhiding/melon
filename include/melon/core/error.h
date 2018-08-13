#ifndef MELON_ERROR_H
#define MELON_ERROR_H

#include <stdlib.h>
#include <stdio.h>

namespace melon
{

typedef void (*logger_callback_fp)(const char* message, ...);
extern logger_callback_fp logger_callback;

#ifdef MELON_DEBUG
#define MELON_LOG(...) melon::logger_callback(__VA_ARGS__)
#else
#define MELON_LOG(...)
#endif

#define MELON_ASSERT(cond, ...) do { if (!(cond)) { fprintf(stderr, "ASSERT FAILURE: \"" #cond "\" at line %d in %s. ", __LINE__, __FILE__); fprintf(stderr, "MESSAGE: " __VA_ARGS__ ); exit(0);} } while(0)
#define MELON_STATIC_ASSERT(cond, msg) typedef int _static_assert_##msg[(cond) ? 1 : -1];

}

#endif
