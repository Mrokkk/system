#pragma once

#include <stdint.h>

int font_load(void* framebuffer);
void string_to_framebuffer(int x, int y, char* string, uint32_t fgcolor, uint32_t bgcolor);
