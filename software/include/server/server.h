#ifndef SRC_SERVER
#define SRC_SERVER

#include <unistd.h>
#include <stdint.h>
#include "utils/args.h"

typedef struct pending_call {
    uint64_t time;
    int caller;
    int callee;
} pending_call_t;

typedef struct ongoing_call {
    uint64_t time;
    int caller;
    int callee;
    pid_t pid;
    unsigned short caller_port;
    unsigned short callee_port;
} ongoing_call_t;

typedef struct server {
    server_conf_t* conf;
    int phone_count;
    int sockfd;
    uint16_t phone_numbers[10];
    pending_call_t pending_calls[10];
    ongoing_call_t ongoing_calls[10];
    ongoing_call_t terminating_calls[10];
} server_t;

extern int server_run(int argc, char** argv);

#endif