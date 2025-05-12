#ifndef SRC_AUDIO_BACKEND_H
#define SRC_AUDIO_BACKEND_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "audiobackend/audio.h"
#include "audiobackend/audio_backend_start_info.h"
#include "audiobackend/transfer.h"
#include "audiobackend/ring_buffer.h"

struct audio_backend {
    struct audio_engine audio_engine;
    struct transfer_engine transfer_engine;
    struct ring_buffer captureRB;
    struct ring_buffer playbackRB;
    bool initialised;
    bool started;
};

extern void init_audio_backend(struct audio_backend* backend, struct program_conf* config);
extern void destroy_audio_backend(struct audio_backend* backend);

extern int  audio_backend_start(struct audio_backend* backend, struct audio_backend_start_info* info);
extern int  audio_backend_stop(struct audio_backend* backend);

#endif