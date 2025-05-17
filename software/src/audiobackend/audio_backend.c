#include "common.h"
#include "args.h"
#include "audiobackend/audio.h"
#include "audiobackend/audio_backend.h"
#include "audiobackend/transfer.h"
#include "audiobackend/ring_buffer.h"

void init_audio_backend(audio_backend_t* backend_p, intercom_conf_t* config) {
    if (backend_p->initialised) {
        return;
    }

    backend_p->impl = (audio_backend_impl_t*)create_shared_memory(sizeof(*backend_p->impl));
    if (backend_p->impl == NULL) {
        error("Failed to create shared memory for audio backend");
    }

    audio_backend_impl_t* backend = backend_p->impl;

    info("Initialising ring buffers");
    init_ring_buffer_shr(&backend->captureRB);
    init_ring_buffer_shr(&backend->playbackRB);

    info("Initialising audio engine");
    init_audio_engine(&backend->audio_engine, &backend->playbackRB, &backend->captureRB, config);

    info("Initialising transfer engine");
    init_transfer_engine(&backend->transfer_engine, &backend->playbackRB, &backend->captureRB, config);

    backend_p->initialised = true;
}

void destroy_audio_backend(audio_backend_t* backend) {
    if (!backend->initialised) {
        return;
    }

    info("Destroying transfer engine");
    destroy_transfer_engine(&backend->impl->transfer_engine);

    info("Destroying audio engine");
    destroy_audio_engine(&backend->impl->audio_engine);
    
    info("Destroying ring buffers");
    destroy_ring_buffer(&backend->impl->captureRB);
    destroy_ring_buffer(&backend->impl->playbackRB);

    destroy_shared_memory(backend->impl, sizeof(*backend->impl));

    backend->initialised = false;
}

int audio_backend_start(audio_backend_t* backend_p, audio_backend_start_info_t* info) {
    int res;

    audio_backend_impl_t* backend = backend_p->impl;

    if (!backend_p->initialised) {
        return ST_NOT_INITIALISED;
    }

    if (backend_p->started) {
        return ST_GOOD;
    }

    if ((res = audio_engine_start(&backend->audio_engine)) != ST_GOOD) {
        warn("Failed to start audio engine with code : %d", res);
        return res;
    }

    if ((res = transfer_engine_start(&backend->transfer_engine, info)) != ST_GOOD) {
        warn("Failed to start transfer engine with code : %d", res);
        return res;
    }

    backend_p->started = true;

    return ST_GOOD;
}

int audio_backend_stop(audio_backend_t* backend_p) {
    audio_backend_impl_t* backend = backend_p->impl;
    int res;

    if (!backend_p->initialised) {
        return ST_NOT_INITIALISED;
    }

    if (!backend_p->started) {
        return ST_GOOD;
    }

    if ((res = audio_engine_stop(&backend->audio_engine)) != ST_GOOD) {
        warn("Failed to stop audio engine with code : %d", res);
        return res;
    }

    if ((res = transfer_engine_stop(&backend->transfer_engine)) != ST_GOOD) {
        // Inconsistent state
        error("Failed to stop transfer engine with audio engine stopped, code : %d", res);
        return res;
    }

    backend_p->started = false;

    return ST_GOOD;
}
