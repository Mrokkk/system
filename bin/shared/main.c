#include <stdio.h>

extern int test();

int main(int argc, char** argv)
{
    printf("Command:");
    for (int i = 0; i < argc; ++i)
    {
        printf(" %s", argv[i]);
    }
    printf("\nHello world!\n");
    test();
    return 0;
}
