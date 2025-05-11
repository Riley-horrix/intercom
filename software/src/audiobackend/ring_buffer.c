#include "miniaudio.h"
#include "common.h"
#include "audiobackend/audio.h"
#include "audiobackend/ring_buffer.h"

/**
 * Initialise given ring buffer.
 * 
 * Fails if error occurs.
 */
void init_ring_buffer(struct ring_buffer* rb) {
    ma_result res;

    if (rb->initialised) {
        return;
    }

    res = ma_rb_init(BUFFER_SIZE_IN_FRAMES * FRAME_SIZE, NULL, NULL, &rb->impl);

    if (res != MA_SUCCESS) {
        ma_rb_uninit(&rb->impl);
        ma_error("Failed to initialise ring buffer", res);
    }

    rb->initialised = true;
}

/**
 * Uninitialise the given ring buffer.
 */
void destroy_ring_buffer(struct ring_buffer* rb) {
    if (!rb->initialised) {
        return;
    }
    ma_rb_uninit(&rb->impl);
    rb->initialised = false;
}

/**
 * Acquire a read pointer from the ring buffer.
 * 
 * This function puts a pointer to the data into `buffer` and the available
 * space to read in `size`. The number written to `size` will be less than or
 * equal to the original value.
 */
int ring_buffer_acquire_read(struct ring_buffer* rb, size_t* size, void** buffer) {
    return ma_call(ma_rb_acquire_read(&rb->impl, size, buffer));
}

int ring_buffer_commit_read(struct ring_buffer* rb, size_t size){
    return ma_call(ma_rb_commit_read(&rb->impl, size));
}

int ring_buffer_acquire_write(struct ring_buffer* rb, size_t* size, void** buffer) {
    return ma_call(ma_rb_acquire_write(&rb->impl, size, buffer));
}

int ring_buffer_commit_write(struct ring_buffer* rb, size_t size) {
    return ma_call(ma_rb_commit_write(&rb->impl, size));
}

int ring_buffer_seek_read(struct ring_buffer* rb, size_t offset) {
    return ma_call(ma_rb_seek_read(&rb->impl, offset));
}

int ring_buffer_seek_write(struct ring_buffer* rb, size_t offset) {
    return ma_call(ma_rb_seek_write(&rb->impl, offset));
}

int32_t ring_buffer_pointer_distance(struct ring_buffer* rb) {
    return ma_rb_pointer_distance(&rb->impl);
}
