#include <libconfig.h>
#include "utils/args.h"
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "logicbackend/logic_backend.h"
#include "intercom/intercom.h"

int intercom_run(int argc, char** argv) {
    intercom_conf_t config;
    info("Initialising program configuration");
    init_intercom_conf(&config, argc, argv);

    info("Initialising audio backend");
    audio_backend_t* audio = create_shared_memory(sizeof(*audio));
    init_audio_backend(audio, &config);

    struct logic_backend logic;
    // init_logic_backend(&logic, audio, &config);
    (void)logic;
    return 0;
}