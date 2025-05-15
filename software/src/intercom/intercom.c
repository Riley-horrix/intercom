#include <libconfig.h>
#include "args.h"
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "logicbackend/logic_backend.h"
#include "intercom/intercom.h"

int intercom_run(int argc, char** argv) {
    struct program_conf config;
    init_program_conf(&config);
    
    if (argc > 1) {
        info("Reading %d program arguments", argc);
        parse_args(&config, argc, argv);
    }

    if (config.config_file != NULL) {
        info("Reading from config file %s", config.config_file);
        read_config_file(&config, config.config_file);
    }

    info("Initialising audio backend");
    struct audio_backend* audio = create_shared_memory(sizeof(*audio));
    init_audio_backend(audio, &config);

    struct logic_backend logic;
    // init_logic_backend(&logic, audio, &config);
    (void)logic;
    return 0;
}