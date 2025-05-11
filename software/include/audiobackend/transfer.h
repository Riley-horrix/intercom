#ifndef SRC_TRANSFER_H
#define SRC_TRANSFER_H

#include <pthread.h>
#include <unistd.h>
#include "audiobackend/audio_backend.h"
#include "audiobackend/ring_buffer.h"

/**
 * Most of the information here is read only for the child loop.
 * 
 * The child loop can only call the thread safe functions on the ring buffers
 * and read the values.
 * 
 * Additionally, since this is shared between processes, it must be allocated
 * using shared memory.
 */
struct transfer_engine {
    struct ring_buffer* playback;
    struct ring_buffer* capture;
    struct audio_backend_start_info info;
    pthread_cond_t startCond;
    pthread_mutex_t startMut; // Owned by child proc
    pid_t procID;
    bool started;
};

extern void init_transfer_engine(struct transfer_engine* engine, struct ring_buffer* captureRB, struct ring_buffer* playbackRB, struct program_conf* config);
extern void destroy_transfer_engine(struct transfer_engine* engine);


extern int transfer_engine_start(struct transfer_engine* engine, struct audio_backend_start_info* info);
extern int transfer_engine_stop(struct transfer_engine* engine);


#endif