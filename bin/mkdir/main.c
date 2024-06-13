#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (mkdir(argv[1], 0))
    {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    return 0;
}
