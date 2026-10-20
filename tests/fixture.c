#include <stdio.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
    if (argc == 4 && strstr(argv[2], "delay")) {
        struct timespec duration = {.tv_sec = 0, .tv_nsec = 400000000};
        nanosleep(&duration, NULL);
    }
    for (int i = 0; i < argc; ++i) puts(argv[i]);
    return 0;
}
