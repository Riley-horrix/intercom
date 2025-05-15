#include <libconfig.h>
#include "args.h"
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "logicbackend/logic_backend.h"
#include "intercom/intercom.h"

int intercom_run(int argc, char** argv) {
    struct program_conf config;
    info("Initialising program configuration");
    init_program_conf(&config, argc, argv);

    info("Initialising audio backend");
    struct audio_backend* audio = create_shared_memory(sizeof(*audio));
    init_audio_backend(audio, &config);

    struct logic_backend logic;
    // init_logic_backend(&logic, audio, &config);
    (void)logic;
    return 0;
}