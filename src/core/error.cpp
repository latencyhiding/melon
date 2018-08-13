#include <melon/core/error.h>

////////////////////////////////////////////////////////////////////////////////
// Default logging callbacks
////////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>

namespace melon
{

static void default_logger(const char* message, ...)
{
    va_list arg_list;
    va_start(arg_list, message);
    vfprintf(stderr, message, arg_list);
    va_end(arg_list);
}

logger_callback_fp logger_callback = default_logger;

}