#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#ifdef RASPBERRY_PI
#define INTERCOM_RPI_FUNCTION
#define INTERCOM_FUNCTION __attribute((unused))
#else
#define INTERCOM_RPI_FUNCTION __attribute((unused))
#define INTERCOM_FUNCTION
#endif

#define error(fmt, ...) (fprintf(stderr, "\e[31m[ERROR]\e[0m " fmt "!\n" , ##__VA_ARGS__), raise(SIGTERM))
#define warn(fmt, ...) fprintf(stdout, "\e[93m[WARN]\e[0m " fmt "!\n" , ##__VA_ARGS__)
#define info(fmt, ...) fprintf(stdout, "\e[32m[info]\e[0m " fmt ".\n" , ##__VA_ARGS__)
#define prompt(fmt, ...) fprintf(stdout, "\e[96m" fmt "\e[0m" , ##__VA_ARGS__)

#define stl_error(code, fmt, ...) error(fmt ", cause: %s" , ##__VA_ARGS__, strerror(code))
#define stl_warn(code, fmt, ...) warn(fmt ", cause: %s" , ##__VA_ARGS__, strerror(code))

#define BIT(n) (0x1 << n)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

enum ST_CODE {
    ST_GOOD            = 0,
    ST_MALLOC_FAIL     = 1,
    ST_INVALID_ARG     = 2,
    ST_FAIL            = 3,
    ST_NOT_INITIALISED = 4
};    

extern void* create_shared_memory(size_t size);
extern int destroy_shared_memory(void* memory, size_t size);

#endif