#ifndef SRC_RING_BUFFER_H
#define SRC_RING_BUFFER_H

#include <stdbool.h>
#include "audio.h"

// Defines for the miniaudio ring buffer
#define RING_BUFFER_SIZE_IN_SECONDS 2
#define BUFFER_SIZE_IN_FRAMES (SAMPLE_RATE * RING_BUFFER_SIZE_IN_SECONDS)

struct ring_buffer {
    ma_pcm_rb impl;
    bool initialised;
};

extern int  init_ring_buffer(struct ring_buffer* rb);
extern void destroy_ring_buffer(struct ring_buffer* rb);


#endif