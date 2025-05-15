#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>

#ifdef RASPBERRY_PI
#include <endian.h>
#else
#include <machine/endian.h>
#endif

#include <arpa/inet.h>
#include <wiringPi.h>

#include "args.h"
#include "common.h"
#include "audiobackend/audio_backend.h"
#include "logicbackend/packets.h"
#include "logicbackend/logic_backend.h"

struct server_info {

};

struct state_t {
    int(*start)(struct state_t** current);
};

struct server_state {
    struct state_t state;
    struct logic_backend* logic;
    int sockfd;
};

struct handshake_state {
    struct server_state server;
    struct state_t* wait_for_call;
};

struct wait_for_call_state {
    struct server_state server;
    struct state_t* make_call;
    struct state_t* accept_call;
    int* phone_number;
    uint16_t* server_udp_port;
    int gpio_dial_pin;
};

struct execute_external_call_state {
    struct server_state server;
    struct state_t* call;
    uint16_t* server_udp_port;
    int phone_number;
};

struct execute_ring_state {
    struct server_state server;
    struct state_t* call;
};

struct execute_call_state {
    struct server_state server;
    uint16_t server_udp_port;
};

static int start_server(struct logic_backend* logic);
static int resolve_hostname(const char* hostname, const char* hostport, const struct addrinfo* hints, struct sockaddr* result);

// Misc time functions
static unsigned long ts_to_micros(struct timespec* ts);

// Server state functions
static int handshake_start(struct state_t** state);
static int wait_for_call_start_gpio(struct state_t** state, bool* received);
static int wait_for_call_start(struct state_t** state);

// Server state helpers
static int no_block_check_receive_call(const struct wait_for_call_state* state, bool* received);

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
    if (secrets->server_hostname == NULL || secrets->server_port == NULL) {
        warn("Server hostname or port was NULL");
        return ST_FAIL;
    }

    // Resolve server host name
    struct addrinfo hints;
    memset(&hints, 0x0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    int res;

    if ((res = resolve_hostname(secrets->server_hostname, secrets->server_port, &hints, (struct sockaddr*)&logic->serverAddr)) != ST_GOOD) {
        warn("Failed to resolve hostname %s, port %s", secrets->server_hostname, secrets->server_port);
    }

    hints.ai_socktype = SOCK_DGRAM;

    if ((res = resolve_hostname(secrets->server_hostname, secrets->server_port, &hints, (struct sockaddr*)&logic->udpServerAddr)) != ST_GOOD) {
        warn("Failed to resolve hostname %s, port %s", secrets->server_hostname, secrets->server_port);
    }

    start_server(logic);

    return ST_GOOD;
}

/**
 * Start the backend server. Assumes that the addrinfo structs are initialised
 * in the logic_backend.
 */
static int start_server(struct logic_backend* logic) {
    int res;

    // Initialise a connection
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Check for valid socket id
    if (sockfd < 0) {
        stl_warn(errno, "Failed to initialise socket with code : %d", sockfd);
        return ST_FAIL;
    }

    // Connect the receive socket
    if ((res = connect(sockfd, (struct sockaddr*)&logic->serverAddr, sizeof(logic->serverAddr)))) {
        stl_warn(errno, "Failed to connect to server");
        res = ST_FAIL;
        goto logic_server_cleanup;
    }

    // Initialise states
    struct server_state server;
    server.logic = logic;
    server.sockfd = sockfd;

    struct handshake_state handshake;
    handshake.server = server;
    handshake.server.state.start = &handshake_start;

    struct wait_for_call_state waitForCall;
    waitForCall.server = server;
    #ifdef RASPBERRY_PI
    waitForCall.server.state.start = &wait_for_call_start_gpio;
#else
    waitForCall.server.state.start = &wait_for_call_start;
#endif
    waitForCall.gpio_dial_pin = logic->hardware.dial_gpio_pin;

    struct execute_external_call_state externalCall;
    struct execute_ring_state ringBell;
    
    struct execute_call_state executeCall;

    // Link together states
    handshake.wait_for_call = (struct state_t*)&waitForCall;

    waitForCall.make_call = (struct state_t*)&externalCall;
    waitForCall.accept_call = (struct state_t*)&ringBell;

    externalCall.call = (struct state_t*)&executeCall;

    ringBell.call = (struct state_t*)&executeCall;

    struct state_t* currentState = (struct state_t*)&handshake;

    res = ST_GOOD;

    while (res == ST_GOOD) {
        res = currentState->start(&currentState);
    }

logic_server_cleanup:
    close(sockfd);
    return res;
}

static unsigned long ts_to_micros(struct timespec* ts) {
    return ts->tv_sec / 1000 + ts->tv_nsec * 1000000;
}

/**
 * Resolve a host name and port to an addrinfo struct.
 * 
 * Note that the pointer `result` must point to the correct addrinfo struct for
 * the socket type in hints, i.e. if the type is AF_INET, then it must point to
 * an addrinfo_in
 */
static int resolve_hostname(const char* hostname, const char* hostport, const struct addrinfo* hints, struct sockaddr* result) {
    // Info returned as a linked list, the next item in results.ai_next
    struct addrinfo* foundAddr;
    struct addrinfo* saved;

    int ret;
    if ((ret = getaddrinfo(hostname, hostport, hints, &foundAddr))) {
        warn("Failed to get address information for server: %s on port %s", hostname, hostport);
        return ST_FAIL;
    }

    saved = foundAddr;

    // Find the first available ip address from hostname.
    while (foundAddr != NULL) {
        // Ensure address is ipv4
        if (foundAddr->ai_family != hints->ai_family || foundAddr->ai_socktype != hints->ai_socktype) {
            foundAddr = foundAddr->ai_next;
            continue;
        }

        // Extract address
        if (foundAddr->ai_family == AF_INET) {
            *(struct sockaddr_in*)result = *(struct sockaddr_in*)foundAddr->ai_addr;
        } else {
            warn("Family not recognised in addrinfo");
            foundAddr = foundAddr->ai_next;
            continue;
        }

        *result = *foundAddr->ai_addr;

        char addrStr[39];

        if (foundAddr->ai_family == AF_INET) {
            inet_ntop(foundAddr->ai_family, &((struct sockaddr_in*)foundAddr->ai_addr)->sin_addr, addrStr, sizeof(addrStr));
        }
        
        info("Resolved hostname %s at %s on port %hu, socket type %s", hostname, addrStr, ntohs(((struct sockaddr_in*)foundAddr->ai_addr)->sin_port), foundAddr->ai_socktype == SOCK_STREAM ? "SOCK_STREAM" : "SOCK_DGRAM");

        break;
    }

    freeaddrinfo(saved);

    return foundAddr != NULL ? ST_GOOD : ST_FAIL;
}

static int handshake_start(struct state_t** state) {
    ssize_t res;
    const struct handshake_state* handshake_state = (struct handshake_state*)*state;

    info("Entered state handshake");

    // Send handshake message
    struct handshake_request req;
    req.id = HANDSHAKE_REQUEST;

    strcpy(req.magic, HANDSHAKE_MAGIC);
    res = send(handshake_state->server.sockfd, (void*)&req, sizeof(req), 0);

    if (res != sizeof(req)) {
        stl_warn(errno, "Failed to send handshake request");
        return ST_FAIL;
    }

    // Listen for response
    struct handshake_request resp;

    res = read(handshake_state->server.sockfd, &resp, sizeof(resp));

    if (res != sizeof(req)) {
        stl_warn(errno, "Failed to receive handshake request");
        return ST_FAIL;
    }

    if (resp.id != HANDSHAKE_RESPONSE) {
        warn("Handshake response id invalid");
        return ST_FAIL;
    }

    // Validate response
    if (strncmp(HANDSHAKE_MAGIC, resp.magic, sizeof(resp.magic)) != 0) {
        warn("Handshake response magic invalid");
        return ST_FAIL;
    }

    // All good so move to next state
    *state = handshake_state->wait_for_call;
    return ST_GOOD;
}

static int no_block_check_receive_call(const struct wait_for_call_state* state, bool* received) {
    // Get received data
    struct incoming_call call;

    // Non-blocking read on the socket
    int res = recv(state->server.sockfd, &call, sizeof(call), MSG_DONTWAIT);

    if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Socket error
        stl_warn(errno, "Socket error reading incoming call");
        return ST_FAIL;
    }

    if (res == -1 || res != sizeof(call)) {
        // No valid data
        return ST_GOOD;
    } else {
        if (ntohs(call.id) != INCOMING_CALL) {
            // Invalid packet but we dont care
            return ST_GOOD;
        }

        *state->phone_number = ntohs(call.from_phone_number);
        *state->server_udp_port = ntohs(call.udp_server_port);

        info("Received call from %d", *state->phone_number);
        *received = true;

        return ST_GOOD;
    }
}

// Also needs a function for checking for server messages at, for example, 2Hz
static int INTERCOM_RPI_FUNCTION wait_for_call_start_gpio(struct state_t** state, bool* received) {
    const struct wait_for_call_state* wait_for_call_state = (struct wait_for_call_state*)*state;

    info("Entered state wait for call start gpio");

    // Listen for changes on the gpio pin
    int pulses = 0;

    // According to the hardware spec, the pin should be normally grounded 
    int pinState;
    int debouncedState = LOW;

    // If the pin is up, then wait for it to fall
    int attempts = 0;
    while ((pinState = digitalRead(wait_for_call_state->gpio_dial_pin)) == HIGH) {
        warn("Dial pin is supposed to be normally grounded, it was high");
        usleep(200 * 1000);

        if (attempts++ >= 10) {
            return ST_FAIL;
        }
    }

    // TODO For now this only allows single digit dialing.
    // 1ms
    const long debounceUs = 1000;
    // 1s
    const long callCommitUs = 1000000;

    unsigned long lastChangeUs;

    struct timespec ts;

    int res = clock_gettime(CLOCK_REALTIME, &ts);
    if (res != 0) {
        stl_warn(errno, "Failed to initialise clock time");
        return ST_FAIL;
    }

    lastChangeUs = 0;

    // pinState is LOW when entering this function
    int pinRead;
    while (true) {
        // Get the iteration time
        int res = clock_gettime(CLOCK_REALTIME, &ts);
        if (res != 0) {
            stl_warn(errno, "Failed to initialise clock time");
            return ST_FAIL;
        }
        unsigned long currentUs = ts_to_micros(&ts);

        if ((pinRead = digitalRead(wait_for_call_state->gpio_dial_pin)) != pinState) {
            pinState = pinRead;
            lastChangeUs = currentUs;
        }

        if (currentUs - lastChangeUs > debounceUs) {
            debouncedState = pinState;
            if (debouncedState == LOW) {
                pulses++;
            }
        }

        if (debouncedState == LOW && currentUs - lastChangeUs > callCommitUs) {
            // Transition to make call state
            *wait_for_call_state->phone_number = pulses - 1;
            *state = wait_for_call_state->make_call;

            info("Calling number %d", *wait_for_call_state->phone_number);
            return ST_GOOD;
        }

        // Check for incoming calls
        bool received_call;
        no_block_check_receive_call(wait_for_call_state, &received_call);
        
        if (received_call) {
            // Transition to accept call state
            *state = wait_for_call_state->accept_call;
            return ST_GOOD;
        }
        
        // Sleep so we dont thrash
        usleep(50);
    }
}

static int INTERCOM_FUNCTION wait_for_call_start(struct state_t** state) {
    struct wait_for_call_state* wait_for_call_state = (struct wait_for_call_state*)state;

    info("Entered state wait for call start");

    int res;

    // Nonblocking read of stdin
    struct pollfd stdinPoll;
    stdinPoll.fd = STDIN_FILENO;
    stdinPoll.events = POLLIN;

    res = poll(&stdinPoll, 1, 0);

    if (res != 1) {
        stl_warn(errno, "Error when polling stdin");
    } else {
        if (stdinPoll.revents & POLLIN) {
            char stdinBuf[16] = { 0 };
            res = read(STDIN_FILENO, stdinBuf, sizeof(stdinBuf));
            char* endPtr;

            int phone_number = (int)strtoul(stdinBuf, &endPtr, 10);

            if (endPtr != stdinBuf) {
                info("Calling number %d", phone_number);

                *wait_for_call_state->phone_number = phone_number;
                *state = wait_for_call_state->make_call;
            }
        }
    }

    // Check for incoming calls
    bool received_call;
    no_block_check_receive_call(wait_for_call_state, &received_call);
    
    if (received_call) {
        // Transition to accept call state
        *state = wait_for_call_state->accept_call;
        return ST_GOOD;
    }

    return ST_GOOD;
}

// Secure communication design

// First do a diffie-hellman exchange with server to obtain a secret key
// Use secret key to secure all future comms

// UDP and main TCP comms to use ChaCha20 / Salsa20 / **Blowfish** (OpenSSL) encryption (for speed)