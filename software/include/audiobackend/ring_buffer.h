#ifndef SRC_RING_BUFFER_H
#define SRC_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include "miniaudio.h"

/**
 * The following is a ring buffer for a single producer thread and a single
 * consumer thread.
 */

typedef struct ring_buffer {
    ma_rb impl;
    void* shrBuffer; // Shared memory buffer (NULLABLE)
    int size;
    bool initialised;
} ring_buffer_t;

extern void init_ring_buffer(ring_buffer_t* rb);
extern void init_ring_buffer_shr(ring_buffer_t* rb);
extern void destroy_ring_buffer(ring_buffer_t* rb);

extern int ring_buffer_acquire_read(ring_buffer_t* rb, size_t* size, void** buffer);
extern int ring_buffer_commit_read(ring_buffer_t* rb, size_t size);
extern int ring_buffer_acquire_write(ring_buffer_t* rb, size_t* size, void** buffer);
extern int ring_buffer_commit_write(ring_buffer_t* rb, size_t size);

extern int     ring_buffer_seek_read(ring_buffer_t* rb, size_t offset);
extern int     ring_buffer_seek_write(ring_buffer_t* rb, size_t offset);
extern int32_t ring_buffer_pointer_distance(ring_buffer_t* rb);

#endif