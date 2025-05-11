#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "audiobackend/audio_backend.h"
#include "audiobackend/transfer.h"

static void handle_child_signal(int sigid);
static void transfer_engine_main(struct transfer_engine* engine);

void init_transfer_engine(struct transfer_engine* engine, struct ring_buffer* captureRB, struct ring_buffer* playbackRB, struct program_conf* config) {
    if (captureRB == NULL || playbackRB == NULL) {
        error("Ring buffers point to NULL in transfer engine");
    }

    engine->capture = captureRB;
    engine->playback = playbackRB;

    int err;

    if (err = pthread_mutex_init(&engine->startMut, NULL)) {
        error("Failed to initialise transfer mutex");
    }

    if (err = pthread_cond_init(&engine->startCond, NULL)) {
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
        info("Started child with pid %ld", pid);
        engine->procID = pid;
        return;
    } else {
        // Child thread
        transfer_engine_main(engine);

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

void transfer_engine_start(struct transfer_engine* engine, struct audio_backend_start_info* info) {
    // Move info into the engine
    memcpy(&engine->info, info, sizeof(struct audio_backend_start_info));

    // Start the process
    engine->started = true;
    pthread_cond_signal(&engine->startCond);
}

void transfer_engine_stop(struct transfer_engine* engine) {
    // Stop the process
    engine->started = false;
}

static void transfer_engine_main(struct transfer_engine* engine) {
    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;

    int sockfd;
    int res;

    while (true) {
        wait_for_start(engine);

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
        // Read existing socket flags
        int flags;
        if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0) {
            warn("Transfer engine couldn't read flags from socket!");
            close(sockfd);
            continue;
        }

        // Make non blocking
        flags |= O_NONBLOCK;

        if (fcntl(sockfd, F_SETFL, flags) < 0) {
            warn("Transfer engine couldn't write flags to socket!");
            close(sockfd);
            continue;
        }
#endif

        sockaddr.sin_port = htons(engine->info.port);
        inet_pton(AF_INET, engine->info.ipv4, &sockaddr.sin_addr);

        // Bind the socket
        if (res = bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) {
            warn("Failed to bind socket to address in transfer engine!");
            close(sockfd);
            continue;
        }

        while (engine->started) {
            // Do a non-blocking read from the port
            // Write the data into the ring buffer

            // Do a write to the port with data from the ring buffer
        }
    }
}

static void wait_for_start(struct transfer_engine* engine) {
    pthread_mutex_lock(&engine->startMut);
    while (!engine->started) {
        pthread_cond_wait(&engine->startCond, &engine->startMut);
    }
    pthread_mutex_unlock(&engine->startMut);
}