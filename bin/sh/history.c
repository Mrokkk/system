#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_SIZE    32

char* history[HISTORY_SIZE];
int history_index = 0;

int history_initialize(void)
{
    for (int i = 0; i < HISTORY_SIZE; history[i++] = malloc(32));
    return 0;
}

int history_add(const char* line)
{
    strcpy(history[history_index++], line);
    // FIXME: use a better container than array
    if (history_index == HISTORY_SIZE) history_index = 0;
    return 0;
}

int c_history(const char*)
{
    for (int i = 0; i < history_index; ++i)
    {
        printf("%u %s\n", i, history[i]);
    }
    return 0;
}
