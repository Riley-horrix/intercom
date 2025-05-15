#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libconfig.h>
#include "common.h"
#include "args.h"

#define conf_fail() error("One or more critical configs could not be found")

static int config_get_u16(struct config_t* conf, const char* path, unsigned short* ret);
static int config_get_str(struct config_t* conf, const char* path, char* ret, ssize_t maxlen);

#define USAGE_STR "Usage: %s [options] [-f <config-file>]"

#define HELP_STR USAGE_STR "\n\n"\
    "[options]:\n"\
    "-f file    Specify the configuration file\n"\
    "-d         Use system default audio settings\n"\
    "-h         Print this message\n"\
    "\n"

void init_program_conf(struct program_conf* config, int argc, char** argv) {
    memset(&config->config_file, 0, sizeof(config->config_file));
    memset(&config->server_hostname, 0, sizeof(config->server_hostname));
    config->phone_number = 0;
    config->server_port = 0;
    config->use_defaults = false;

    int opt;

    if (argc <= 1) {
        warn("No config file specified");
        error(USAGE_STR, argv[0]);
    }

    bool validConfigFile = false;
    bool printHelp = false;

    while ((opt = getopt(argc, argv, "hdf:")) != -1) {
        switch (opt) {
        case 'd': config->use_defaults = true; break;
        case 'f': strncpy(config->config_file, optarg, sizeof(config->config_file)); validConfigFile = true; break;
        case 'h': printHelp = true; break;
        default: error(USAGE_STR, argv[0]);
        }
    }

    if (printHelp) {
        // info("As");
        printf("\n"HELP_STR, argv[0]);
        exit(0);
    }

    if (!validConfigFile) {
        warn("No config file specified");
        error(USAGE_STR, argv[0]);
    }
    
    struct config_t libconf;
    config_init(&libconf);
    int err = config_read_file(&libconf, config->config_file);
    
    if (err != CONFIG_TRUE) {
        warn("Error occurred int line %d while parsing %s, %s", config_error_line(&libconf), config_error_file(&libconf), config_error_text(&libconf));
    }

    int conferr = 0;

    conferr |= config_get_str(&libconf, "/app/server_hostname", config->server_hostname, sizeof(config->server_hostname));
    conferr |= config_get_u16(&libconf, "/app/server_port", &config->server_port);
    conferr |= config_get_u16(&libconf, "/app/phone_number", &config->phone_number);
    
    config_destroy(&libconf);

    if (conferr != ST_GOOD) {
        conf_fail();
    }
}

static int config_get_u16(struct config_t* conf, const char* path, unsigned short* ret) {
    int value;
    int err = config_lookup_int(conf, path, &value);

    if (err == CONFIG_TRUE) {
        if (value > 0 || value > USHRT_MAX) {
            info("Config found: %s = %d", path, value);
            *ret = (unsigned short)value;
            return ST_GOOD;
        } else {
            warn("Invalid config found: %s = %d", path, value);
        }
    } else {
        warn("Config not found: %s", path);
    }

    return ST_FAIL;
}

static int config_get_str(struct config_t* conf, const char* path, char* ret, ssize_t maxlen) {
    const char* value;
    int err = config_lookup_string(conf, "/app/server_hostname", &value);

    if (err == CONFIG_TRUE) {
        if (value != NULL) {
            info("Config found: %s = %s", path, value);
            strncpy(ret, value, maxlen);
            return ST_GOOD;
        } else {
            warn("Invalid config found: %s = %s", path, value);
        }
    } else {
        warn("Config not found: %s", path);
    }

    return ST_FAIL;
}