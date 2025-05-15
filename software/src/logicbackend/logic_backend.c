#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
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
    int sockfd;
};

struct handshake_state {
    struct server_state server;
    struct state_t* wait_for_call;
};

struct wait_for_call_state {
    struct server_state server;
    struct state_t* begin_call;
    int gpio_dial_pin;
    int* phone_number;
};

struct execute_call_state {
    struct server_state server;
    struct state_t* begin_call;
    int phone_number;
};

static int start_server(struct logic_backend* logic);
static int resolve_hostname(const char* hostname, const char* hostport, const struct addrinfo* hints, struct sockaddr* result);

// Misc time functions
static unsigned long ts_to_micros(struct timespec* ts);

// Server state functions
static int handshake_start(struct state_t** state);
static int wait_for_call_start_gpio(struct state_t** state);
static int wait_for_call_start(struct state_t** state);

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
        goto logic_server_cleanup;
    }

    // Commence handshake with the server
    struct handshake_state handshake;

    // Wait for the hardware device to commence a call
    struct wait_for_call_state waitForCall;

#ifdef RASPBERRY_PI
    handshake.wait_for_call = wait_for_call_start_gpio;
#else
    handshake.wait_for_call = wait_for_call_start;
#endif

    // Execute the call
    // struct begin_call beginCall;

    struct state_t* currentState = &handshake;

    res = ST_GOOD;

    while (res == ST_GOOD) {
        currentState->start(&currentState);
    }

logic_server_cleanup:
    close(sockfd);
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

    // Validate response
    if (strncmp(HANDSHAKE_MAGIC, resp.magic, sizeof(resp.magic)) != 0) {
        warn("Handshake request magic invalid");
        return ST_FAIL;
    }

    // All good so move to next state
    *state = handshake_state->wait_for_call;
}

// Also needs a function for checking for server messages at, for example, 2Hz
static int wait_for_call_start_gpio(struct state_t** state) {
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

    // 1ms
    const long debounceUs = 1000;
    // 1s
    const long callCommitUs = 1000000;

    unsigned long currentUs;
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
            // Send the call
            *state = wait_for_call_state->begin_call;
            return;
        }

        // Sleep so we dont thrash the gpio
        usleep(100);
    }
}


// Secure communication design

// First do a diffie-hellman exchange with server to obtain a secret key
// Use secret key to secure all future comms

// UDP and main TCP comms to use ChaCha20 / Salsa20 / **Blowfish** (OpenSSL) encryption (for speed)