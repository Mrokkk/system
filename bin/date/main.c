#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    time_t ts = time(NULL);
    printf("%u\n", ts);
    return 0;
}
