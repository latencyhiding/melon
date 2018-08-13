#include <melon/core/error.h>
#include <melon/gfx.h>

#include <stdint.h>
#include <tinycthread.h>

namespace melon
{

namespace gfx
{

handle invalid_handle;

size_t vertex_data_type_bytes(const vertex_data_type type)
{
    switch (type)
    {
        case MELON_FORMAT_BYTE:
        case MELON_FORMAT_UBYTE: return sizeof(uint8_t);
        case MELON_FORMAT_SHORT:
        case MELON_FORMAT_USHORT: return sizeof(uint16_t);
        case MELON_FORMAT_INT:
        case MELON_FORMAT_UINT: return sizeof(uint32_t);
        case MELON_FORMAT_HALF: return sizeof(float) / 2;
        case MELON_FORMAT_FLOAT: return sizeof(float);
        default: return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

const device_params* default_gfx_device_params()
{
    static device_params* p_default_device_params = NULL;
    static device_params  default_device_params   = {};

    if (!p_default_device_params)
    {
        default_device_params.resource_count = {};
        {
            default_device_params.resource_count.max_shaders         = 256;
            default_device_params.resource_count.max_buffers         = 256;
            default_device_params.resource_count.max_pipelines       = 256;
            default_device_params.resource_count.max_command_buffers = 256;
        }
        default_device_params.allocator = *(default_cb_allocator());
        p_default_device_params         = &default_device_params;
    };
    return p_default_device_params;
}

}    // namespace gfx
}    // namespace melon