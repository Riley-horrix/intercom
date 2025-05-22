#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "audiobackend/transfer.h"

#include "miniaudio.h"
#include "audiobackend/audio.h"

static void handle_child_signal(int sigid);
static int  wait_for_start(struct transfer_engine* engine);
static void transfer_engine_main(struct transfer_engine* engine);
static void transfer_engine_debug(struct transfer_engine* engine);

bool childKilled = false;

void init_transfer_engine(struct transfer_engine* engine, ring_buffer_t* playback, ring_buffer_t* capture, intercom_conf_t* config) {
    if (capture == NULL || playback == NULL) {
        error("Ring buffers point to NULL in transfer engine");
    }

    engine->capture = capture;
    engine->playback = playback;
    engine->started = false;

    int err;

    // Initialise the attributes
    pthread_mutexattr_t attr;
    if ((err = pthread_mutexattr_init(&attr))) {
        stl_error(err, "Failed to initialise mutex attributes");
    }

    pthread_condattr_t condAttr;
    if ((err = pthread_condattr_init(&condAttr))) {
        stl_error(err, "Failed to initialise condition attributes");
    }

    // Set the mutex and condition variable to function in shared memory
    if ((err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED))) {
        stl_error(err, "Failed to set shared status on mutex");
    }
    if ((err = pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED))) {
        stl_error(err, "Failed to set shared status on condition");
    }

    // Initialise the mutex and condition variable
    if ((err = pthread_mutex_init(&engine->startMut, &attr))) {
        error("Failed to initialise transfer mutex");
    }

    if ((err = pthread_cond_init(&engine->startCond, &condAttr))) {
        error("Failed to initialise transfer condition");
    }

    // Destroy the attributes
    if ((err = pthread_mutexattr_destroy(&attr))) {
        stl_error(err, "Failed to destroy mutex attributes");
    }
    if ((err = pthread_condattr_destroy(&condAttr))) {
        stl_error(err, "Failed to destroy condition attributes");
    }

    // Register child signal handler
    struct sigaction sa;
    sa.sa_handler = handle_child_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if ((err = sigaction(SIGCHLD, &sa, NULL))) {
        stl_error(err, "Failed to register signal handler in transfer engine");
    }

    // For the process
    pid_t pid = fork();

    if (pid == -1) {
        stl_error(errno, "Transfer engine failed to fork");
    }

    if (pid != 0) {
        // Parent thread
        info("Started child with pid %d", pid);
        engine->procID = pid;
        return;
    } else {
        // Start child thread running main engine
        (void)transfer_engine_main;
        transfer_engine_debug(engine);
            
        // This should never be reached - thread should be killed in main func
        return;
    }
}

void destroy_transfer_engine(struct transfer_engine* engine) {
    int err;
    
    // Kill the child process
    if (!childKilled && (err = kill(engine->procID, SIGSEGV))) {
        stl_warn(errno, "Transfer engine could not kill child");
    }

    if ((err = pthread_mutex_destroy(&engine->startMut))) {
        stl_warn(err, "Failed to destroy transfer mutex");
    }

    if ((err = pthread_cond_destroy(&engine->startCond))) {
        stl_warn(err, "Failed to destroy transfer condition");
    }
}

int transfer_engine_start(struct transfer_engine* engine, audio_backend_start_info_t* info) {
    int res;
    // Move info into the engine
    memcpy(&engine->info, info, sizeof(audio_backend_start_info_t));

    // Start the process
    if ((res = pthread_mutex_lock(&engine->startMut))) {
        stl_warn(res, "Could not lock transfer engine mutex");
        return ST_FAIL;
    }

    engine->started = true;
    info("Transfer engine started");

    if ((res = pthread_cond_signal(&engine->startCond))) {
        stl_warn(res, "Could not signal transfer engine condition");
        return ST_FAIL;
    }

    if ((res = pthread_mutex_unlock(&engine->startMut))) {
        stl_warn(res, "Could not unlock transfer engine mutex");
        return ST_FAIL;
    }

    return ST_GOOD;
}

int transfer_engine_stop(struct transfer_engine* engine) {
    // Stop the process
    int res;

    if ((res = pthread_mutex_lock(&engine->startMut))) {
        stl_warn(res, "Could not lock transfer engine mutex");
        return ST_FAIL;
    }

    engine->started = false;
    info("Engine stopped");

    if ((res = pthread_mutex_unlock(&engine->startMut))) {
        stl_warn(res, "Could not unlock transfer engine mutex");
        return ST_FAIL;
    }

    return ST_GOOD;
}

static void transfer_engine_main(struct transfer_engine* engine) {
    struct sockaddr_in recvAddr;
    struct sockaddr_in savedRecvAddr;
    struct sockaddr_in sendAddr;

    recvAddr.sin_family = AF_INET;
    sendAddr.sin_family = AF_INET;

    int res;

    while (true) {
        if (!engine->started) {
            wait_for_start(engine);
        }

        // Socket information in engine->info now valid
        // Initialise sockets
        int sockfd = socket(
            AF_INET, 
#ifdef linux
            SOCK_DGRAM | SOCK_NONBLOCK, 
#else
            SOCK_DGRAM,
#endif
            0);

        // Check for valid socket id
        if (sockfd < 0) {
            stl_warn(errno, "Failed to initialise socket with code : %d", sockfd);
            continue;
        }

        // Have to set non blocking manually on macos
#ifndef linux
        // Read existing socket flags
        int flags;
        if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0) {
            stl_warn(errno, "Transfer engine couldn't read flags from socket!");
            goto transfer_engine_cleanup;
        }

        // Make non blocking
        flags |= O_NONBLOCK;

        if (fcntl(sockfd, F_SETFL, flags) < 0) {
            stl_warn(errno, "Transfer engine couldn't write flags to socket!");
            goto transfer_engine_cleanup;
        }
#endif

        recvAddr.sin_port = engine->info.recvPort;
        sendAddr.sin_port = engine->info.sendPort;
        if ((inet_pton(AF_INET, engine->info.recvAddr, &recvAddr.sin_addr))) {
            stl_error(errno, "Failed to convert receive address to number");
        }

        if ((inet_pton(AF_INET, engine->info.sendAddr, &sendAddr.sin_addr))) {
            stl_error(errno, "Failed to convert send address to number");
        }


        // Bind the receive socket
        if ((res = connect(sockfd, (struct sockaddr*)&recvAddr, sizeof(recvAddr)))) {
            stl_warn(errno, "Failed to connect socket to address in transfer engine");
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
                    stl_warn(errno, "Transfer engine recfrom failed");
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
                    stl_warn(errno, "Transfer engine sendto failed");
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
        if ((close(sockfd))) {
            stl_warn(errno, "Failed to close socket");
        }
    }
}


static void transfer_engine_debug(struct transfer_engine* engine) {
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
            if (wait_for_start(engine) != ST_GOOD) {
                error("Transfer engine child process couldn't wait for engine start");
            }
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

static int wait_for_start(struct transfer_engine* engine) {
    int res;

    if ((res = pthread_mutex_lock(&engine->startMut))) {
        stl_warn(res, "Child could not lock transfer engine mutex");
        return ST_FAIL;
    }

    while (!engine->started) {
        if ((res = pthread_cond_wait(&engine->startCond, &engine->startMut))) {
            stl_warn(res, "Child could not wait on condition!");
            return ST_FAIL;
        }
    }

    if ((res = pthread_mutex_unlock(&engine->startMut))) {
        stl_warn(res, "Child could not unlock transfer engine mutex");
        return ST_FAIL;
    }

    return ST_GOOD;
}

/**
 * This handler should only handle SIGCHLD.
 */
static void handle_child_signal(int sigid) {
    childKilled = true;
}