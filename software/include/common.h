#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <stdio.h>
#include <signal.h>

#define error(fmt, ...) (fprintf(stderr, "\e[31m[ERROR]\e[0m " fmt "!\n", ##__VA_ARGS__), raise(SIGTERM))
#define warn(fmt, ...) fprintf(stdout, "\e[93m[WARN]\e[0m " fmt "!\n", ##__VA_ARGS__)
#define info(fmt, ...) fprintf(stdout, "\e[32m[info]\e[0m " fmt ".\n", ##__VA_ARGS__)
#define prompt(fmt, ...) fprintf(stdout, "\e[96m" fmt "\e[0m", ##__VA_ARGS__)

#define BIT(n) (0x1 << n)

enum ST_CODE {
    ST_GOOD        = 0,
    ST_MALLOC_FAIL = 1,
    ST_INVALID_ARG = 2,
    ST_FAIL   = 3
};    

extern const char* errorStatus;

#endif