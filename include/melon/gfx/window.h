#ifndef MELON_WINDOW_H
#define MELON_WINDOW_H

#include <melon/core/handle.h>
#include <melon/core/memory.h>
#include <stdbool.h>

typedef struct melon_window melon_window;

typedef struct
{
    size_t input_buffer_capacity;
    melon_allocator_api allocator; 
} melon_input_params;

const melon_input_params* melon_default_input_params();

typedef enum
{
    MELON_NONE,

    MELON_WINDOW_MOVED,
    MELON_WINDOW_RESIZED,
    MELON_WINDOW_CLOSED,
    MELON_WINDOW_FOCUSED,
    MELON_WINDOW_UNFOCUSED,
    MELON_WINDOW_SCALE_CHANGED,

    MELON_MOUSE_BUTTON_PRESSED,
    MELON_MOUSE_BUTTON_JUST_PRESSED,
    MELON_MOUSE_BUTTON_RELEASED,
    MELON_MOUSE_MOVED,
    MELON_MOUSE_SCROLLED,

    MELON_KEY_PRESSED,
    MELON_KEY_JUST_PRESSED,
    MELON_KEY_REPEATED,
    MELON_KEY_RELEASED,

    MELON_JOYSTICK_CONNECTED,
    MELON_JOYSTICK_DISCONNECTED,
    MELON_JOYSTICK_AXIS_MOVED,
    MELON_JOYSTICK_BUTTON_PRESSED,
    MELON_JOYSTICK_BUTTON_JUST_PRESSED,
    MELON_JOYSTICK_BUTTON_RELEASED
} melon_input_event_type;

typedef enum
{
    // Key codes
    MELON_KEY_UNKNOWN,
    MELON_KEY_SPACE,
    MELON_KEY_APOSTROPHE,
    MELON_KEY_COMMA,
    MELON_KEY_MINUS,
    MELON_KEY_PERIOD,
    MELON_KEY_SLASH,
    MELON_KEY_0,
    MELON_KEY_1,
    MELON_KEY_2,
    MELON_KEY_3,
    MELON_KEY_4,
    MELON_KEY_5,
    MELON_KEY_6,
    MELON_KEY_7,
    MELON_KEY_8,
    MELON_KEY_9,
    MELON_KEY_SEMICOLON,
    MELON_KEY_EQUAL,
    MELON_KEY_A,
    MELON_KEY_B,
    MELON_KEY_C,
    MELON_KEY_D,
    MELON_KEY_E,
    MELON_KEY_G,
    MELON_KEY_H,
    MELON_KEY_I,
    MELON_KEY_J,
    MELON_KEY_K,
    MELON_KEY_L,
    MELON_KEY_M,
    MELON_KEY_N,
    MELON_KEY_O,
    MELON_KEY_P,
    MELON_KEY_Q,
    MELON_KEY_R,
    MELON_KEY_S,
    MELON_KEY_T,
    MELON_KEY_U,
    MELON_KEY_V,
    MELON_KEY_W,
    MELON_KEY_X,
    MELON_KEY_Y,
    MELON_KEY_Z,
    MELON_KEY_LEFT_BRACKET,
    MELON_KEY_BACKSLASH,
    MELON_KEY_RIGHT_BRACKET,
    MELON_KEY_GRAVE_ACCENT,
    MELON_KEY_WORLD_1,
    MELON_KEY_WORLD_2,
    MELON_KEY_ESCAPE,
    MELON_KEY_ENTER,
    MELON_KEY_TAB,
    MELON_KEY_BACKSPACE,
    MELON_KEY_INSERT,
    MELON_KEY_DELETE,
    MELON_KEY_RIGHT,
    MELON_KEY_LEFT,
    MELON_KEY_DOWN,
    MELON_KEY_PAGE_UP,
    MELON_KEY_PAGE_DOWN,
    MELON_KEY_HOME,
    MELON_KEY_END,
    MELON_KEY_CAPS_LOCK,
    MELON_KEY_SCROLL_LOCK,
    MELON_KEY_NUM_LOCK,
    MELON_KEY_PRINT_SCREEN,
    MELON_KEY_PAUSE,
    MELON_KEY_F1,
    MELON_KEY_F2,
    MELON_KEY_F3,
    MELON_KEY_F4,
    MELON_KEY_F5,
    MELON_KEY_F6,
    MELON_KEY_F7,
    MELON_KEY_F8,
    MELON_KEY_F9,
    MELON_KEY_F10,
    MELON_KEY_F11,
    MELON_KEY_F12,
    MELON_KEY_F13,
    MELON_KEY_F14,
    MELON_KEY_F15,
    MELON_KEY_F16,
    MELON_KEY_F17,
    MELON_KEY_F18,
    MELON_KEY_F19,
    MELON_KEY_F20,
    MELON_KEY_F21,
    MELON_KEY_F22,
    MELON_KEY_F23,
    MELON_KEY_F24,
    MELON_KEY_F25,
    MELON_KEY_KP_0,
    MELON_KEY_KP_1,
    MELON_KEY_KP_2,
    MELON_KEY_KP_3,
    MELON_KEY_KP_4,
    MELON_KEY_KP_5,
    MELON_KEY_KP_6,
    MELON_KEY_KP_7,
    MELON_KEY_KP_9,
    MELON_KEY_KP_DECIMAL,
    MELON_KEY_KP_DIVIDE,
    MELON_KEY_KP_MULTIPLY,
    MELON_KEY_KP_SUBTRACT,
    MELON_KEY_KP_ADD,
    MELON_KEY_KP_ENTER,
    MELON_KEY_KP_EQUAL,
    MELON_KEY_LEFT_SHIFT,
    MELON_KEY_LEFT_CONTROL,
    MELON_KEY_LEFT_ALT,
    MELON_KEY_LEFT_SUPER,
    MELON_KEY_RIGHT_SHIFT,
    MELON_KEY_RIGHT_CONTROL,
    MELON_KEY_RIGHT_ALT,
    MELON_KEY_RIGHT_SUPER,
    MELON_KEY_MENU
} melon_key_code;

typedef enum
{
    MELON_MOUSE_BUTTON_1,
    MELON_MOUSE_BUTTON_2,
    MELON_MOUSE_BUTTON_3,
    MELON_MOUSE_BUTTON_4,
    MELON_MOUSE_BUTTON_5,
    MELON_MOUSE_BUTTON_6,
    MELON_MOUSE_BUTTON_7,
    MELON_MOUSE_BUTTON_8,
    MELON_MOUSE_BUTTON_LEFT   = MELON_MOUSE_BUTTON_1,
    MELON_MOUSE_BUTTON_RIGHT  = MELON_MOUSE_BUTTON_2,
    MELON_MOUSE_BUTTON_MIDDLE = MELON_MOUSE_BUTTON_3
} melon_mouse_button;

typedef struct
{
    melon_input_event_type type;
    union {
        melon_window* window;
        int           joystick;
    };

    union {
        struct
        {
            int x, y;
        } window_moved;
        struct
        {
            int w, h;
        } window_resized;
        struct
        {
            float w, h;
        } window_scale_changed;
        struct
        {
            melon_mouse_button button;
        } mouse_button_event;
        struct
        {
            float x, y;
        } mouse_moved;
        struct
        {
            int x, y;
        } mouse_scrolled;
        struct
        {
            melon_key_code key;
        } key_event;
        struct
        {
            float new_value;
        } axis_moved;
        struct
        {
            int button;
        } joystick_button_event;
    };
} melon_input_event;

void melon_poll_input_events();
const melon_input_event* melon_pop_input_event();

// Window backend specific functions
melon_window* melon_create_window(int width, int height, const char* title);
void          melon_destroy_window(melon_window* window);

bool melon_window_should_close(melon_window* window);
void melon_swap_buffers(melon_window* window);

#endif