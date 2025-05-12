#ifndef SRC_RING_BUFFER_H
#define SRC_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include "miniaudio.h"
#include "audiobackend/audio.h"

/**
 * The following is a ring buffer for a single producer thread and a single
 * consumer thread.
 */

// Defines for the miniaudio ring buffer
#define RING_BUFFER_SIZE_IN_SECONDS 2
#define BUFFER_SIZE_IN_FRAMES (SAMPLE_RATE * RING_BUFFER_SIZE_IN_SECONDS)

struct ring_buffer {
    ma_rb impl;
    void* shrBuffer; // Shared memory buffer (NULLABLE)
    int size;
    bool initialised;
};

extern void init_ring_buffer(struct ring_buffer* rb);
extern void init_ring_buffer_shr(struct ring_buffer* rb);
extern void destroy_ring_buffer(struct ring_buffer* rb);

extern int ring_buffer_acquire_read(struct ring_buffer* rb, size_t* size, void** buffer);
extern int ring_buffer_commit_read(struct ring_buffer* rb, size_t size);
extern int ring_buffer_acquire_write(struct ring_buffer* rb, size_t* size, void** buffer);
extern int ring_buffer_commit_write(struct ring_buffer* rb, size_t size);

extern int     ring_buffer_seek_read(struct ring_buffer* rb, size_t offset);
extern int     ring_buffer_seek_write(struct ring_buffer* rb, size_t offset);
extern int32_t ring_buffer_pointer_distance(struct ring_buffer* rb);


#endif