#include "miniaudio.h"
#include "common.h"
#include "audio.h"
#include "ring_buffer.h"

int init_ring_buffer(struct ring_buffer* rb) {
    if (rb->initialised) {
        return ST_GOOD;
    }

    ma_result result = ma_pcm_rb_init(FORMAT, CHANNELS, BUFFER_SIZE_IN_FRAMES, NULL, NULL, &rb->impl);
    rb->initialised = result == MA_SUCCESS;

    return rb->initialised ? ST_GOOD : ST_MINI_FAIL;
}

void destroy_ring_buffer(struct ring_buffer* rb) {
    if (!rb->initialised) {
        return;
    }
    ma_pcm_rb_uninit(&rb->impl);
    rb->initialised = false;
}
