#include "common.h"
#include "args.h"
#include "audiobackend/audio.h"
#include "audiobackend/transfer.h"
#include "audiobackend/ring_buffer.h"
#include "audiobackend/audio_backend.h"

void init_audio_backend(struct audio_backend* backend, struct program_conf* config) {
    if (backend->initialised) {
        return ST_GOOD;
    }

    info("Initialising ring buffers");
    init_ring_buffer(&backend->captureRB);
    init_ring_buffer(&backend->playbackRB);

    info("Initialising audio engine");
    init_audio_engine(&backend->audio_engine, &backend->playbackRB, &backend->captureRB, config);

    info("Initialising transfer engine");
    init_transfer_engine(&backend->audio_engine, &backend->playbackRB, &backend->captureRB, config);

    backend->initialised = true;
}

void destroy_audio_backend(struct audio_backend* backend) {
    if (!backend->initialised) {
        return ST_GOOD;
    }

    info("Destroying ring buffers");
    destroy_transfer_engine(&backend->transfer_engine);
    destroy_audio_engine(&backend->audio_engine);

    info("Destroying audio engine");
    destroy_ring_buffer(&backend->captureRB);

    info("Destroying transfer engine");
    destroy_ring_buffer(&backend->playbackRB);

    backend->initialised = false;
}

int audio_backend_start(struct audio_backend* backend, struct audio_backend_start_info* info) {
    int res;

    if (!backend->initialised) {
        return ST_NOT_INITIALISED;
    }

    if (backend->started) {
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

    backend->started = true;

    return ST_GOOD;
}

int audio_backend_stop(struct audio_backend* backend) {
    int res;

    if (!backend->initialised) {
        return ST_NOT_INITIALISED;
    }

    if (!backend->started) {
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

    backend->started = false;

    return ST_GOOD;
}
