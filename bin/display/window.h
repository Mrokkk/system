#pragma once

#include "list.h"
#include "object.h"
#include "vector2.h"

struct window;
typedef struct window window_t;

struct window
{
    list_head_t objects;
    list_head_t dirty;
    vector2_t position;
    vector2_t size;
    char* title;
};

static inline void window_object_add(window_t* window, object_t* object)
{
    list_add_tail(&object->list, &window->objects);
    object->window = window;
}

extern window_t* main_window;

static inline void main_window_set(window_t* window)
{
    main_window = window;
}
