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

void* create_shared_memory(size_t size) {
    // Our memory buffer will be readable and writable:
    int protection = PROT_READ | PROT_WRITE;

    // The buffer will be shared (meaning other processes can access it), but
    // anonymous (meaning third-party processes cannot obtain an address for it),
    // so only this process and its children will be able to use it:
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    // The remaining parameters to `mmap()` are not important for this use case,
    // but the manpage for `mmap` explains their purpose.
    return mmap(NULL, size, protection, visibility, -1, 0);
}

void terminate(int num) {
    (void)num;
    destroy_audio_backend(engine);
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