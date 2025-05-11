#ifndef SRC_ARGS_H
#define SRC_ARGS_H

#include <stdbool.h>

struct program_conf {
    bool useDefaults;
};

extern void parse_args(struct program_conf* conf, int argc, char** argv);

#endif