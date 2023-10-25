#include "object.h"

#include "targa.h"
#include "utils.h"
#include "window.h"

void object_draw(object_t* o, bool is_pressed)
{
    tga_to_framebuffer(
        is_pressed && o->pressed_img ? o->pressed_img : o->normal_img,
        o->position.x, o->position.y, &vinfo, buffer);

    list_add_tail(&o->dirty, &o->window->dirty);
}
