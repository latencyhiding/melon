#ifndef MELON_WINDOW_BACKEND_H
#define MELON_WINDOW_BACKEND_H

#include <melon/gfx/window.h>

bool melon_window_backend_init();
void melon_window_backend_destroy();

bool melon_input_init(const melon_input_params* config);
void melon_input_destroy();

bool melon_push_input_event(const melon_input_event* input_event);

#endif