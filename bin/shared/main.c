#include <stdio.h>

extern int test();
extern int a;

int main()
{
    printf("Hello world!\n");
    printf("Test returned: %u, %u\n", test(), a);
    return 0;
}
