#pragma once

#include <kernel/list.h>

struct video_driver
{
    const char* name;
    int         score;
    int (*initialize)(void);
    list_head_t list_entry;
};

typedef struct video_driver video_driver_t;

int video_driver_register(video_driver_t* driver);
