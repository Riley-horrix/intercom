#ifndef SRC_ARGS_H
#define SRC_ARGS_H

#include <libconfig.h>
#include <stdbool.h>

struct program_conf {
    char config_file[128];
    char server_hostname[128];
    unsigned short phone_number;
    unsigned short server_port;
    bool use_defaults;
};

extern void init_program_conf(struct program_conf* config, int argc, char** argv);

#endif