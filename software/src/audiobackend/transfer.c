#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define DEBUG
#ifdef DEBUG
#include "miniaudio.h"
#include "audiobackend/audio.h"
#endif
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "audiobackend/transfer.h"

static void handle_child_signal(int sigid);
static void wait_for_start(struct transfer_engine* engine);
static void transfer_engine_main(struct transfer_engine* engine);
static void transfer_engine_waveform(struct transfer_engine* engine);

void init_transfer_engine(struct transfer_engine* engine, struct ring_buffer* captureRB, struct ring_buffer* playbackRB, struct program_conf* config) {
    if (captureRB == NULL || playbackRB == NULL) {
        error("Ring buffers point to NULL in transfer engine");
    }

    engine->capture = captureRB;
    engine->playback = playbackRB;
    engine->started = false;

    int err;

    if ((err = pthread_mutex_init(&engine->startMut, NULL))) {
        error("Failed to initialise transfer mutex");
    }

    if ((err = pthread_cond_init(&engine->startCond, NULL))) {
        error("Failed to initialise transfer condition");
    }

    // Register any child signals    
    if (signal(SIGCHLD, &handle_child_signal) == SIG_ERR) {
        error("Transfer engine failed to register signal handler");
    }

    // For the process
    pid_t pid = fork();

    if (pid == -1) {
        error("Transfer engine failed to fork");
    }

    if (pid != 0) {
        // Parent thread
        info("Started child with pid %d", pid);
        engine->procID = pid;
        return;
    } else {
        // Child thread
        transfer_engine_waveform(engine);

        if (false) {
            transfer_engine_main(engine);
        }
            
        // This should never be reached - thread should be killed in main func
        return;
    }
}

void destroy_transfer_engine(struct transfer_engine* engine) {
    // Kill the child process
    int err;
    if ((err = pthread_mutex_destroy(&engine->startMut))) {
        warn("Failed to destroy transfer mutex");
    }

    if ((err = pthread_cond_destroy(&engine->startCond))) {
        warn("Failed to destroy transfer condition");
    }

}

int transfer_engine_start(struct transfer_engine* engine, struct audio_backend_start_info* info) {
    // Move info into the engine
    memcpy(&engine->info, info, sizeof(struct audio_backend_start_info));

    // Start the process
    pthread_mutex_lock(&engine->startMut);
    engine->started = true;
    pthread_mutex_unlock(&engine->startMut);
    
    pthread_cond_broadcast(&engine->startCond);
    return ST_GOOD;
}

int transfer_engine_stop(struct transfer_engine* engine) {
    // Stop the process
    engine->started = false;
    info("stop");

    return ST_GOOD;
}

static void transfer_engine_main(struct transfer_engine* engine) {
    struct sockaddr_in recvAddr;
    struct sockaddr_in savedRecvAddr;
    struct sockaddr_in sendAddr;
    recvAddr.sin_family = AF_INET;
    sendAddr.sin_family = AF_INET;

    int sockfd;
    int res;

    while (true) {
        if (!engine->started) {
            wait_for_start(engine);
        }

        // Socket information in engine->info now valid
        // Initialise sockets
        sockfd = socket(
            AF_INET, 
#ifdef linux
            SOCK_DGRAM | SOCK_NONBLOCK, 
#else
            SOCK_DGRAM,
#endif
            0);

        // Have to set non blocking manually on macos
#ifndef linux

        if (sockfd < 0) {
            warn("Failed to initialise socket with code : %d", sockfd);
            continue;
        }

        // Read existing socket flags
        int flags;
        if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0) {
            warn("Transfer engine couldn't read flags from socket!");
            goto transfer_engine_cleanup;
        }

        // Make non blocking
        flags |= O_NONBLOCK;

        if (fcntl(sockfd, F_SETFL, flags) < 0) {
            warn("Transfer engine couldn't write flags to socket!");
            goto transfer_engine_cleanup;
        }
#endif

        recvAddr.sin_port = htons(engine->info.recvPort);
        sendAddr.sin_port = htons(engine->info.sendPort);
        inet_pton(AF_INET, engine->info.recvAddr, &recvAddr.sin_addr);
        inet_pton(AF_INET, engine->info.sendAddr, &sendAddr.sin_addr);

        // Bind the receive socket
        if ((res = bind(sockfd, (struct sockaddr*)&recvAddr, sizeof(recvAddr)))) {
            warn("Failed to bind socket to address in transfer engine");
            goto transfer_engine_cleanup;
        }

        ssize_t written;
        size_t len;
        socklen_t addrSize;
        void* buffer;
        while (engine->started) {
            // Request size for the ring buffer
            len = TRANSFER_REQUEST_SIZE;
            if (ring_buffer_acquire_write(engine->playback, &len, &buffer) != ST_GOOD) {
                warn("Transfer engine failed to obtain write pointer to capture buffer");
                continue;
            }

            // Do a non-blocking read from the socket
            // Write the data into the ring buffer
            written = recvfrom(sockfd, buffer, len, 0, (struct sockaddr*)&savedRecvAddr, &addrSize);
            if (written == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Bad error 
                    warn("Transfer engine recfrom failed with code %d", errno);
                }
                // No data available in non blocking so just go again
                goto transfer_engine_do_write;
            }

            // Commit the write
            if (ring_buffer_commit_write(engine->playback, written) != ST_GOOD) {
                warn("Transfer engine failed to commit the write");
            }

transfer_engine_do_write:
            // Request size for the ring buffer
            len = TRANSFER_REQUEST_SIZE;

            // Get a pointer to the ring buffer
            if (ring_buffer_acquire_read(engine->capture, &len, &buffer) != ST_GOOD) {
                warn("Transfer engine failed to obtain read pointer to capture buffer");
                continue;
            }

            // Send data from the ring buffer
            written = sendto(sockfd, buffer, len, 0, (const struct sockaddr*)&sendAddr, sizeof(sendAddr));
            if (written == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Bad error 
                    warn("Transfer engine sendto failed with code %d", errno);
                }
                continue;
            }

            // Commit the read 
            if (ring_buffer_commit_read(engine->capture, written) != ST_GOOD) {
                warn("Transfer engine failed to commit the read");
            }
        }
transfer_engine_cleanup:
        // Close the socket
        close(sockfd);
    }
}

#ifdef DEBUG

static void transfer_engine_waveform(struct transfer_engine* engine) {
    ma_waveform_config config = ma_waveform_config_init(
        FORMAT,
        CHANNELS,
        SAMPLE_RATE,
        ma_waveform_type_sine,
        0.1, 
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
        // info("here");
        if (!engine->started) {
            // wait_for_start(engine);
        }

        // 'Read' data from the capture ring buffer
        ring_buffer_seek_read(engine->capture, ring_buffer_pointer_distance(engine->capture));

        // If data left in read buffer is less than a constant, write data in
        if ((dist = ring_buffer_pointer_distance(engine->playback)) < bufferMin) {
            size_t toWrite = bufferWrite;
            void* buffer;
            if (ring_buffer_acquire_write(engine->playback, &toWrite, &buffer) != ST_GOOD) {
                warn("Failed to acquire write pointer from ring buffer");
            }
            size_t frameCount = toWrite / FRAME_SIZE;

            if (ma_waveform_read_pcm_frames(&waveform, buffer, frameCount, NULL) != MA_SUCCESS) {
                warn("Failed to read pcm frames");
            }

            if (ring_buffer_commit_write(engine->playback, frameCount * FRAME_SIZE) != ST_GOOD) {
                warn("Failed to commit the ring buffer write");
            }
        }
    }
}

#endif

static void wait_for_start(struct transfer_engine* engine) {
    info("g");
    pthread_mutex_lock(&engine->startMut);
    info("h");
    while (!engine->started) {
        pthread_cond_wait(&engine->startCond, &engine->startMut);
    }
    pthread_mutex_unlock(&engine->startMut);
}

static void handle_child_signal(int sigid) {
    warn("Child was killed :( ");
}