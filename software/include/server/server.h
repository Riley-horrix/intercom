#ifndef SRC_SERVER
#define SRC_SERVER

#include "args.h"

typedef struct server {
    struct server_conf_t* conf;
} server_t;

extern int server_run(int argc, char** argv);

#endif