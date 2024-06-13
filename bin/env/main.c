#include <stdio.h>
#include <unistd.h>

int main(int, char*[], char* envp[])
{
    for (int i = 0; envp[i]; ++i)
    {
        printf("%s\n", envp[i]);
    }
    return 0;
}
