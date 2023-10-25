#pragma once

#include <stdbool.h>

#include "list.h"
#include "vector2.h"

struct object;
struct window;
struct tga_header;

typedef struct object object_t;
struct object
{
    struct tga_header* normal_img;
    struct tga_header* pressed_img;
    vector2_t position;
    vector2_t size;
    int (*on_click)(object_t* object, vector2_t* position);
    struct window* window;
    list_head_t list;
    list_head_t dirty;
};

void object_draw(object_t* o, bool is_pressed);
