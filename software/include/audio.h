#ifndef SRC_AUDIO
#define SRC_AUDIO

#include <stdio.h>

#define AA 2

extern void print_test(void);

static void header_testing(void) {
    printf("header test %d\n", AA);
}

#endif