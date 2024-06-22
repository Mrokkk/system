#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void alarm_cb(int)
{
    printf("timer triggered\n");
}

int main()
{
    signal(SIGALRM, alarm_cb);
    alarm(3);
    timer_t id;
    if (timer_create(CLOCK_MONOTONIC, NULL, &id))
    {
        perror("timer_create");
    }
    else
    {
        struct itimerspec dur = {
            .it_interval = {.tv_sec = 2,},
            .it_value = {.tv_sec = 5,}
        };

        timer_settime(id, 0, &dur, NULL);
    }
    while (1)
    {
    }
    return 0;
}
