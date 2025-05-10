#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"
#include "ring_buffer.h"
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

    // Setup termination handler
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &terminate;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    info("Initialising ring buffers");
    

    if (init_ring_buffer(&captureRB) != ST_GOOD ||
        init_ring_buffer(&captureRB) != ST_GOOD) {
        
        error("Failed to initialise ring buffers");
    }

    info("Initialising audio engine");
    
    if (init_audio_engine(&engine, &playbackRB, &captureRB) != ST_GOOD) {
        error("Audio engine failed to initialise");
    }

    info("Initialising backend engine");

    terminate(0);
}