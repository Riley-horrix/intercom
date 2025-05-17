#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libconfig.h>
#include "common.h"
#include "args.h"

#define conf_fail() error("One or more critical configs could not be found")
#define set_if_fail(err, bool) if ((err) != ST_GOOD){ bool = true;}

static void init_config_from_file(struct config_t* config, const char* path);
static int config_get_u16(struct config_t* conf, const char* path, unsigned short* ret);
static int config_get_str(struct config_t* conf, const char* path, char* ret, ssize_t maxlen);
static int config_get_bool(struct config_t* conf, const char* path, bool* ret);

#define INTERCOM_USAGE_STR "Usage: %s [options] [-f <config-file>]"

#define INTERCOM_HELP_STR INTERCOM_USAGE_STR "\n\n"\
    "[options]:\n"\
    "-f file    Specify the configuration file\n"\
    "-d         Use system default audio settings\n"\
    "-h         Print this message\n"\
    "\n"

#define SERVER_USAGE_STR "Usage: %s [options] [-f <config-file>]"

#define SERVER_HELP_STR SERVER_USAGE_STR "\n\n"\
    "[options]:\n"\
    "-f file    Specify the configuration file\n"\
    "-h         Print this message\n"\
    "\n"

void init_intercom_conf(intercom_conf_t* config, int argc, char** argv) {
    memset(&config->config_file, 0, sizeof(config->config_file));
    memset(&config->server_hostname, 0, sizeof(config->server_hostname));
    config->phone_number = 0;
    config->server_port = 0;
    config->use_audio_defaults = false;

    int opt;

    if (argc <= 1) {
        warn("No config file specified");
        error(INTERCOM_USAGE_STR, argv[0]);
    }

    bool validConfigFile = false;
    bool printHelp = false;

    while ((opt = getopt(argc, argv, "hdf:")) != -1) {
        switch (opt) {
        case 'd': config->use_audio_defaults = true; break;
        case 'f': strncpy(config->config_file, optarg, sizeof(config->config_file)); validConfigFile = true; break;
        case 'h': printHelp = true; break;
        default: error(INTERCOM_USAGE_STR, argv[0]);
        }
    }

    if (printHelp) {
        info("\n" INTERCOM_HELP_STR, argv[0]);
        exit(0);
    }

    if (!validConfigFile) {
        warn("No config file specified");
        error(INTERCOM_USAGE_STR, argv[0]);
    }
    
    struct config_t libconf;
    init_config_from_file(&libconf, config->config_file);

    bool configFail = false;

    // Parse required configurations
    set_if_fail(config_get_str(&libconf, "/app/server_hostname", config->server_hostname, sizeof(config->server_hostname)), configFail)
    set_if_fail(config_get_u16(&libconf, "/app/server_port", &config->server_port), configFail)
    set_if_fail(config_get_u16(&libconf, "/app/phone_number", &config->phone_number), configFail)
    
    // Parse optional arguments
    config_get_bool(&libconf, "/app/use_audio_defaults", &config->use_audio_defaults);

    config_destroy(&libconf);

    if (configFail) {
        conf_fail();
    }
}

void init_server_conf(server_conf_t* config, int argc, char ** argv) {
    memset(&config->config_file, 0, sizeof(config->config_file));
    config->server_port = 0;
    config->audio_port_min = 0;
    config->audio_port_max = 0;

    int opt;

    if (argc <= 1) {
        warn("No config file specified");
        error(SERVER_USAGE_STR, argv[0]);
    }

    bool validConfigFile = false;
    bool printHelp = false;

    while ((opt = getopt(argc, argv, "hdf:")) != -1) {
        switch (opt) {
        case 'f': strncpy(config->config_file, optarg, sizeof(config->config_file)); validConfigFile = true; break;
        case 'h': printHelp = true; break;
        default: error(SERVER_USAGE_STR, argv[0]);
        }
    }

    if (printHelp) {
        info("\n" SERVER_HELP_STR, argv[0]);
        exit(0);
    }

    if (!validConfigFile) {
        warn("No config file specified");
        error(SERVER_USAGE_STR, argv[0]);
    }

    struct config_t libconf;
    init_config_from_file(&libconf, config->config_file);

    bool configFail = false;

    // Parse required arguments
    set_if_fail(config_get_u16(&libconf, "/app/server_port", &config->server_port), configFail);
    set_if_fail(config_get_u16(&libconf, "/app/audio_port_min", &config->audio_port_min), configFail);
    set_if_fail(config_get_u16(&libconf, "/app/audio_port_max", &config->audio_port_max), configFail);

    // Parse optional arguments


    config_destroy(&libconf);

    if (configFail) {
        conf_fail();
    }
}

static void init_config_from_file(struct config_t* config, const char* path) {
    config_init(config);
    int err = config_read_file(config, path);
    
    if (err != CONFIG_TRUE) {
        warn("Error occurred int line %d while parsing %s, %s", config_error_line(config), config_error_file(config), config_error_text(config));
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
    int err = config_lookup_string(conf, path, &value);

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

static int config_get_bool(struct config_t* conf, const char* path, bool* ret) {
    int value;
    int err = config_lookup_bool(conf, path, &value);

    if (err == CONFIG_TRUE) {
        *ret = value != 0;
        info("Config found: %s = %s", path, *ret ? "true" : "false");
        return ST_GOOD;
    } else {
        warn("Config not found: %s", path);
    }

    return ST_FAIL;
}