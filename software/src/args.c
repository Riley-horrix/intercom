#include <unistd.h>
#include <stdbool.h>
#include <libconfig.h>
#include "common.h"
#include "args.h"

void init_program_conf(struct program_conf* conf) {
    conf->config_file = NULL;
    conf->server_hostname = NULL;
    conf->server_port = 0;
    conf->use_defaults = false;
}

void parse_args(struct program_conf* conf, int argc, char** argv) {
    int opt;
    while ((opt = getopt(argc, argv, "df:")) != -1) {
        switch (opt) {
        case 'd': conf->use_defaults = true; break;
        case 'f': conf->config_file = optarg; break;
        default:
            error("Usage: %s [-d] [-f config-file]", argv[0]);
        }
    }
}

void read_config_file(struct program_conf* programConfig, const char* path) {
    struct config_t config;
    config_init(&config);

    int err;

    err = config_read_file(&config, path);

    if (err != CONFIG_TRUE) {
        warn("Error occurred int line %d while parsing %s, %s", config_error_line(&config), path, config_error_text(&config));
        goto cleanup;
    }


cleanup:
    config_destroy(&config);
}