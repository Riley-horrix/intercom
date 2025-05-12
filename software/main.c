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

struct audio_backend* engine;

void terminate(int num) {
    (void)num;
    destroy_audio_backend(engine);
    destroy_shared_memory(engine, sizeof(*engine));
    exit(num);
}

int main(int argc, char** argv) {
    // Parse arguments
    struct program_conf conf;
    parse_args(&conf, argc, argv);

    engine = (struct audio_backend*)create_shared_memory(sizeof(*engine));

    info("Initialising audio backend");
    init_audio_backend(engine, &conf);

    struct audio_backend_start_info info;
    audio_backend_start(engine, &info);

    signal(SIGINT, terminate);

    while (true) { }

    destroy_audio_backend(engine);
}