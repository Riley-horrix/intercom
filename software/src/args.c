#include <unistd.h>
#include <stdbool.h>
#include "common.h"
#include "args.h"

void parse_args(struct program_conf* conf, int argc, char** argv) {
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd': conf->useDefaults = true; break;
        default:
            error("Usage: %s [-d]", argv[0]);
        }
    }
}