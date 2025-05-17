#ifndef SRC_ARGS_H
#define SRC_ARGS_H

#include <libconfig.h>
#include <stdbool.h>

typedef struct intercom_conf {
    // Required
    char config_file[128];
    char server_hostname[128];
    unsigned short phone_number;
    unsigned short server_port;

    // Optional
    bool use_audio_defaults;
} intercom_conf_t;

typedef struct server_conf {
    // Required
    char config_file[128];
    unsigned short server_port;
    unsigned short audio_port_min;
    unsigned short audio_port_max;
} server_conf_t;

extern void init_intercom_conf(intercom_conf_t* config, int argc, char** argv);

extern void init_server_conf(server_conf_t* config, int argc, char** argv);

#endif