#include "miniaudio.h"
#include "common.h"
#include "audiobackend/audio.h"
#include "audiobackend/ring_buffer.h"

// Defines for the miniaudio ring buffer
#define RING_BUFFER_SIZE_IN_SECONDS 2
#define BUFFER_SIZE_IN_FRAMES (SAMPLE_RATE * RING_BUFFER_SIZE_IN_SECONDS)

/**
 * Initialise given ring buffer.
 * 
 * Fails if error occurs.
 */
void init_ring_buffer(ring_buffer_t* rb) {
    ma_result res;

    if (rb->initialised) {
        return;
    }

    rb->size = BUFFER_SIZE_IN_FRAMES * FRAME_SIZE;
    rb->shrBuffer = NULL;

    res = ma_rb_init(rb->size, NULL, NULL, &rb->impl);

    if (res != MA_SUCCESS) {
        ma_rb_uninit(&rb->impl);
        ma_error("Failed to initialise ring buffer", res);
    }

    rb->initialised = true;
}

/**
 * Initialise a ring buffer using shared memory.
 * 
 * Fails if error occurs.
 */
void init_ring_buffer_shr(ring_buffer_t* rb) {
    ma_result res;

    if (rb->initialised) {
        return;
    }

    // Allocate buffer
    rb->size = BUFFER_SIZE_IN_FRAMES * FRAME_SIZE;
    rb->shrBuffer = create_shared_memory(rb->size);

    if ((res = ma_rb_init(rb->size, rb->shrBuffer, NULL, &rb->impl)) != MA_SUCCESS) {
        ma_rb_uninit(&rb->impl);
        ma_error("Failed to initialise ring buffer", res);
    }

    rb->initialised = true;
}

/**
 * Uninitialise the given ring buffer.
 */
void destroy_ring_buffer(ring_buffer_t* rb) {
    if (!rb->initialised) {
        return;
    }

    ma_rb_uninit(&rb->impl);

    if (rb->shrBuffer != NULL) {
        destroy_shared_memory(rb->shrBuffer, rb->size);
    }

    rb->initialised = false;
}

/**
 * Acquire a read pointer from the ring buffer.
 * 
 * This function puts a pointer to the data into `buffer` and the available
 * space to read in `size`. The number written to `size` will be less than or
 * equal to the original value.
 */
int ring_buffer_acquire_read(ring_buffer_t* rb, size_t* size, void** buffer) {
    return ma_call(ma_rb_acquire_read(&rb->impl, size, buffer));
}

int ring_buffer_commit_read(ring_buffer_t* rb, size_t size){
    return ma_call(ma_rb_commit_read(&rb->impl, size));
}

int ring_buffer_acquire_write(ring_buffer_t* rb, size_t* size, void** buffer) {
    return ma_call(ma_rb_acquire_write(&rb->impl, size, buffer));
}

int ring_buffer_commit_write(ring_buffer_t* rb, size_t size) {
    return ma_call(ma_rb_commit_write(&rb->impl, size));
}

int ring_buffer_seek_read(ring_buffer_t* rb, size_t offset) {
    return ma_call(ma_rb_seek_read(&rb->impl, offset));
}

int ring_buffer_seek_write(ring_buffer_t* rb, size_t offset) {
    return ma_call(ma_rb_seek_write(&rb->impl, offset));
}

int32_t ring_buffer_pointer_distance(ring_buffer_t* rb) {
    return ma_rb_pointer_distance(&rb->impl);
}
