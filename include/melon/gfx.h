#ifndef MELON_GFX_H
#define MELON_GFX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <melon/core.h>
#include <melon/gfx/backend.h>
#include <melon/gfx/window.h>

typedef struct
{
    const melon_device_params* device_params;
    const melon_input_params*  input_params;
} melon_gfx_config;

bool melon_gfx_init(const melon_gfx_config* config);
void melon_gfx_destroy();

#ifdef __cplusplus
}
#endif

#endif
