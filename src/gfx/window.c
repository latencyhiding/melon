#include <melon/gfx/window.h>
#include <melon/core/memory.h>

static struct
{
    melon_input_event* event_buffer;
    size_t head;
    size_t tail;
    size_t capacity;
} input_queue;

static melon_input_params config;

const melon_input_params* melon_default_input_params()
{
    static melon_input_params* p_default_input_params = NULL;
    static melon_input_params  default_device_params  = {0};

    if (!p_default_input_params)
    {
        default_device_params.input_buffer_capacity = 256;
        default_device_params.allocator = *(melon_default_cb_allocator());

        p_default_input_params = &default_device_params;
    }
    return p_default_input_params;
}

bool melon_input_init(const melon_input_params* in_config)
{
    if (!in_config)
    {
        config = *(melon_default_input_params());
    }
    else
    {
        config = *in_config;
    }

    input_queue.event_buffer = MELON_ALLOC(
        config.allocator, sizeof(melon_input_event) * (config.input_buffer_capacity + 1), MELON_DEFAULT_ALIGN);
    
    if (!input_queue.event_buffer)
    {
        return false;
    }

    input_queue.head     = 1;
    input_queue.tail     = 0;
    input_queue.capacity = config.input_buffer_capacity;

    return true;
}

void melon_input_destroy() 
{
    MELON_FREE(config.allocator, input_queue.event_buffer);
}

bool melon_push_input_event(const melon_input_event* input_event)
{
    size_t new_head = (input_queue.head + 1) % input_queue.capacity;
    if (new_head == input_queue.tail)
    {
        return false;
    }

    input_queue.event_buffer[input_queue.head] = *input_event;
    input_queue.head = new_head;

    return true;
}

const melon_input_event* melon_pop_input_event() 
{
    if (input_queue.head == input_queue.tail)
    {
        return NULL;
    }

    const melon_input_event* result = input_queue.event_buffer + input_queue.tail;
    input_queue.tail = (input_queue.tail + 1) % input_queue.capacity;

    return result;
}
