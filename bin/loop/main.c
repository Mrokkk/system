#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void alarm_cb()
{
    printf("alarm triggered\n");
}

int main()
{
    signal(SIGALRM, alarm_cb);
    alarm(3);
    while (1)
    {
    }
    return 0;
}
