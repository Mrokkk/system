#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LEN 128

int main(int argc, char** argv)
{
    int res;
    char pathname[MAX_PATH_LEN];
    const char* module = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (!strchr(argv[i], '-'))
        {
            module = argv[i];
        }
        else
        {
            printf("Unrecognized option: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (!module)
    {
        printf("Module path/name is needed\n");
        return EXIT_FAILURE;
    }

    if (strlen(module) >= MAX_PATH_LEN)
    {
        printf("Name too long: %s\n", module);
        return EXIT_FAILURE;
    }

    if (strchr(module, '/'))
    {
        strcpy(pathname, module);
    }
    else
    {
        strcpy(pathname, "/lib/modules/");
        strcat(pathname, module);
        strcat(pathname, ".ko");
    }

    extern int init_module(const char*);

    if ((res = init_module(pathname)))
    {
        perror(pathname);
        return EXIT_FAILURE;
    }

    return 0;
}

