#ifndef SRC_ARGS_H
#define SRC_ARGS_H

#include <stdbool.h>

struct program_conf {
    const char* config_file;
    bool use_defaults;
    const char* server_hostname;
    unsigned short server_port;
};

extern void init_program_conf(struct program_conf* conf);
extern void parse_args(struct program_conf* conf, int argc, char** argv);

extern void read_config_file(struct program_conf* conf, const char* path);

#endif