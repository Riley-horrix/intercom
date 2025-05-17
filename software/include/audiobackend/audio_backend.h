#ifndef SRC_AUDIO_BACKEND_H
#define SRC_AUDIO_BACKEND_H

#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "audiobackend/audio.h"
#include "audiobackend/audio_backend_start_info.h"
#include "audiobackend/transfer.h"
#include "audiobackend/ring_buffer.h"

typedef struct audio_backend_impl {
    audio_engine_t audio_engine;
    struct transfer_engine transfer_engine;
    ring_buffer_t captureRB;
    ring_buffer_t playbackRB;
} audio_backend_impl_t;

typedef struct audio_backend {
    audio_backend_impl_t* impl;
    bool initialised;
    bool started;
} audio_backend_t;

extern void init_audio_backend(audio_backend_t* backend, intercom_conf_t* config);
extern void destroy_audio_backend(audio_backend_t* backend);

extern int  audio_backend_start(audio_backend_t* backend, audio_backend_start_info_t* info);
extern int  audio_backend_stop(audio_backend_t* backend);

#endif