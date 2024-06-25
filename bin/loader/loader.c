#include <stdio.h>

int main(int argc, char* argv[], char* envp[])
{
    (void)envp; (void)argc;

    printf("Called with:");
    for (int i = 0; i < argc; ++i)
    {
        printf(" %s", argv[i]);
    }
    putchar('\n');

    return 0;
}
