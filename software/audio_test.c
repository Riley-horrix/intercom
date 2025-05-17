#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "args.h"
#include "common.h"
#include "audiobackend/ring_buffer.h"
#include "audiobackend/audio.h"
#include "audiobackend/audio_backend.h"

audio_backend_t engine;

void terminate(int num) {
    destroy_audio_backend(&engine);
    exit(num);
}

int main(int argc, char** argv) {
    // Parse arguments
    intercom_conf_t conf;
    init_intercom_conf(&conf, argc, argv);

    info("Initialising audio backend");
    init_audio_backend(&engine, &conf);

    audio_backend_start_info_t info;
    audio_backend_start(&engine, &info);

    signal(SIGINT, terminate);

    while (true) { }

    destroy_audio_backend(&engine);
}