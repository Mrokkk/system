#include <stdio.h>

#define RESET           "\e[0m"
#define BLACK           "\e[30m"
#define RED             "\e[31m"
#define GREEN           "\e[32m"
#define YELLOW          "\e[33m"
#define BLUE            "\e[34m"
#define MAGENTA         "\e[35m"
#define CYAN            "\e[36m"
#define WHITE           "\e[37m"
#define GRAY            "\e[90m"
#define BRIGHTRED       "\e[91m"
#define BRIGHTGREEN     "\e[92m"
#define BRIGHTYELLOW    "\e[93m"
#define BRIGHTBLUE      "\e[94m"
#define BRIGHTMAGENTA   "\e[95m"
#define BRIGHTCYAN      "\e[96m"
#define BRIGHTWHITE     "\e[97m"

int main()
{
#define COLOR_PRINT(color) \
    printf(color #color RESET "\n")
    COLOR_PRINT(BLACK);
    COLOR_PRINT(RED);
    COLOR_PRINT(GREEN);
    COLOR_PRINT(YELLOW);
    COLOR_PRINT(BLUE);
    COLOR_PRINT(MAGENTA);
    COLOR_PRINT(CYAN);
    COLOR_PRINT(WHITE);
    COLOR_PRINT(GRAY);
    COLOR_PRINT(BRIGHTRED);
    COLOR_PRINT(BRIGHTGREEN);
    COLOR_PRINT(BRIGHTYELLOW);
    COLOR_PRINT(BRIGHTBLUE);
    COLOR_PRINT(BRIGHTMAGENTA);
    COLOR_PRINT(BRIGHTCYAN);
    COLOR_PRINT(BRIGHTWHITE);
    return 0;
}
