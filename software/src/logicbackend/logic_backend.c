#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include "args.h"
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "logicbackend/logic_backend.h"

struct server_info {

};

// static int start_server(struct logic_backend* logic, struct server_info* secrets);

void init_logic_backend(struct logic_backend* logic, struct program_conf* config) {
    info("Initialising audio backend");
    logic->audio = (struct audio_backend*) create_shared_memory(sizeof(struct audio_backend));

    // TODO : Probably move this into the audio_backend
    if (!logic->audio) {
        error("Failed to allocate shared memory");
    }

    init_audio_backend(logic->audio, config);
}

void destroy_logic_backend(struct logic_backend* logic) {
    info("Destroying audio backend");
    destroy_audio_backend(logic->audio);
    destroy_shared_memory(logic->audio, sizeof(struct audio_backend));
}

/**
 * Begin the logic server, blocking call.
 */
int logic_backend_start(struct logic_backend* logic, struct server_secrets* secrets) {
    // Resolve server host name
    struct addrinfo hints;
    memset(&hints, 0x0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    // Info returned as a linked list, the next item in results.ai_next
    struct addrinfo* results;

    int ret;
    if ((ret = getaddrinfo(secrets->server_hostname, secrets->server_port, &hints, &results))) {
        warn("Failed to get address information for server: %s on port %s", secrets->server_hostname, secrets->server_port);
        return ST_FAIL;
    }

    while(results != NULL) {
        info("Found host: %s", results->ai_canonname);
        results = results->ai_next;
    }

    freeaddrinfo(results);

    return 0;
}

// Secure communication design

// First do a diffie-hellman exchange with server to obtain a secret key
// Use secret key to secure all future comms

// UDP and main TCP comms to use ChaCha20 / Salsa20 / **Blowfish** (OpenSSL) encryption (for speed)