#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "ring_buffer.h"
#include "args.h"
#include "common.h"
#include "audio.h"

struct ring_buffer captureRB;
struct ring_buffer playbackRB;

struct audio_engine engine;

// Destroy all allocated structures & processes
void terminate(int signum) {
    printf("\n");
    info("Terminating program with status %d", signum);
    destroy_audio_engine(&engine);
    destroy_ring_buffer(&captureRB);
    destroy_ring_buffer(&playbackRB);

    exit(signum);
}

int main(int argc, char** argv) {
    // Parse arguments
    struct program_conf conf;
    parse_args(&conf, argc, argv);

    // Setup termination handler
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &terminate;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    info("Initialising ring buffers");
    
    init_ring_buffer(&captureRB);
    init_ring_buffer(&playbackRB);

    info("Initialising audio engine");
    
    init_audio_engine(&engine, &playbackRB, &captureRB, &conf);

    // info("Initialising backend engine");

    ma_waveform_config config = ma_waveform_config_init(
        FORMAT,
        CHANNELS,
        SAMPLE_RATE,
        ma_waveform_type_sine,
        0.01, 
        220);
    
    ma_waveform waveform;
    ma_result result = ma_waveform_init(&config, &waveform);
    if (result != MA_SUCCESS) {
        error("Failed to initialise waveform");
    }

    uint32_t bufferMin = 5000; // 5 kb
    uint32_t bufferWrite = 100000; // 100 kb

    int32_t dist;
    
    while (true) {
        // 'Read' data from the capture ring buffer
        ring_buffer_seek_read(&captureRB, ring_buffer_pointer_distance(&captureRB));

        // If data left in read buffer is less than a constant, write data in
        if ((dist = ring_buffer_pointer_distance(&playbackRB)) < bufferMin) {
            size_t toWrite = bufferWrite;
            void* buffer;
            if (ring_buffer_acquire_write(&playbackRB, &toWrite, &buffer) != ST_GOOD) {
                warn("err 1");
            }
            size_t frameCount = toWrite / FRAME_SIZE;
            // uint64_t framesRead = 0;

            if (ma_waveform_read_pcm_frames(&waveform, buffer, frameCount, NULL) != MA_SUCCESS) {
                warn("err 2");
            }

            if (ring_buffer_commit_write(&playbackRB, frameCount * FRAME_SIZE) != ST_GOOD) {
                warn("err 3");
            }
        }
    }

    terminate(0);
}