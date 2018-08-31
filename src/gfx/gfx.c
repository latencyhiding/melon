#include <melon/gfx.h>
#include "window_backend.h"

const melon_gfx_config* melon_default_gfx_params()
{
    static melon_gfx_config default_gfx_config = {0};
    static melon_gfx_config* p_default_gfx_config = NULL;

    if (!p_default_gfx_config)
    {
        default_gfx_config = (melon_gfx_config) {
            .device_params = melon_default_device_params(),
            .input_params  = melon_default_input_params()
        };

        p_default_gfx_config = &default_gfx_config;
    }

    return p_default_gfx_config;
}

bool melon_gfx_init(const melon_gfx_config* in_config)
{
    melon_gfx_config* config;
    if (!in_config)
    {
        config = (melon_gfx_config*) melon_default_gfx_params();
    }
    else
    {
        config = (melon_gfx_config*) in_config;
    }

    bool rc = melon_window_backend_init();
    if (!rc)
    {
        return false;
    }

    return melon_gfx_backend_init(config->device_params)
           && melon_input_init(config->input_params);
}

void melon_gfx_destroy()
{
    melon_gfx_backend_destroy();
    melon_input_destroy();
    melon_window_backend_destroy();
}