#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include "utils/args.h"
#include "server/packets.h"
#include "server/server.h"

#define INTERNET_PROTOCOL AF_INET
#define LOCAL_ADDR "127.0.0.1"

#define MSG_BUFFER_SIZE 1024

// To run the server needs : TCP port, UDP port min, UPD port max

// When a new connection arrives, fork to handle. This child process
// will 'own' the connection with that node. When a call happens the
// child will fork again and start its child to run the udp forwarding

static int init_server(server_t* server);
static int start_server(server_t* server);
static int handle_message_data(server_t* server, uint8_t* buffer, size_t length, size_t* read, struct sockaddr* recvAddress, socklen_t sockaddrLen);

// Message handling functions
static int handle_handshake(server_t* server, uint8_t* buffer, uint8_t len, struct sockaddr* recvAddress, socklen_t sockaddrLen);
static int handle_call_request(server_t* server, uint8_t* buffer, uint8_t len);
static int handle_incoming_response(server_t* server, uint8_t* buffer, uint8_t len);
static int handle_terminate(server_t* server, uint8_t* buffer, uint8_t len);
static int handle_terminate_ack(server_t* server, uint8_t* buffer, uint8_t len);

// Misc
static uint16_t allocate_phone_number(server_t* server, uint16_t requested);

int server_run(int argc, char** argv) {
    int err;
    server_conf_t config;
    info("Initialising server configuration");
    init_server_conf(&config, argc, argv);

    info("Initialising server");
    server_t server;
    server.conf = &config;

    if ((err = init_server(&server)) != ST_GOOD) {
        warn("Failed to initialise server");
        return ST_FAIL;
    }

    info("Starting server");
    return start_server(&server);
}

static int init_server(server_t* server) {
    int err;

    // Address to receive on
    struct sockaddr_in recvAddr;
    recvAddr.sin_family = INTERNET_PROTOCOL;
    recvAddr.sin_port = htons(server->conf->server_port);
    
    inet_pton(INTERNET_PROTOCOL, LOCAL_ADDR, &recvAddr.sin_addr);

    int sockfd = socket(
        INTERNET_PROTOCOL, 
        SOCK_DGRAM,
        0);

    // Check for valid socket id
    if (sockfd < 0) {
        stl_warn(errno, "Failed to initialise socket with code : %d", sockfd);
        close(sockfd);
        return ST_FAIL;
    }

    // Bind socket to local port
    if ((err = bind(sockfd, (const struct sockaddr*)&recvAddr, sizeof(recvAddr))) != 0) {
        stl_warn(errno, "Failed to bind address to socket");
        close(sockfd);
        return ST_FAIL;
    }

    server->sockfd = sockfd;
    server->phone_count = 0;
    return ST_GOOD;
}

/**
 * Main thread server loop. This function will have 1 thread listening for
 * connections on the socket. When one arrives, it will fork off a thread.
 * 
 * The main server thread is stateless, and handles connections with data
 * stored in the server_t.
 */
static int start_server(server_t* server) {
    // Listen for connections and fork off to handle them
    uint8_t msgBuffer[MSG_BUFFER_SIZE];

    // These variables handle overflowing buffers
    // If the buffer has overflowed then the flag is set
    bool overflow = false;

    // Once the data is processed, the remaining bytes are shifted to the top
    // of the array, and writeInd updated to point just after it.
    // Then the next set of data is copied in just after the write pointer.
    size_t writeInd = 0;

    // To handle saving the user address
    struct sockaddr_in recvAddress;

    info("Server started");
    
    while (1) {
        socklen_t addressLen = sizeof(recvAddress);
        ssize_t received = recvfrom(server->sockfd, msgBuffer + writeInd, MSG_BUFFER_SIZE - writeInd, 0, (struct sockaddr*)&recvAddress, &addressLen);

        writeInd = 0;

        // Check for an error, TODO probably dont error out here
        if (received == -1) {
            stl_error(errno, "Server failed to receive data");
        }

        // Check to see if the buffer has 'overflowed'
        if (received == MSG_BUFFER_SIZE) {
            overflow = true;
        }

        // Iterate over the message buffer
        handle_message_data(server, msgBuffer, received, &writeInd, (struct sockaddr*)&recvAddress, addressLen);

        // If buffer overflow, then shift remaining bytes down
        if (overflow && writeInd < received) {
            memmove(msgBuffer, msgBuffer + writeInd, received - writeInd);
        }

        overflow = false;
    }

    return ST_GOOD;
}

static int handle_message_data(server_t* server, uint8_t* buffer, size_t length, size_t* read, struct sockaddr* recvAddress, socklen_t sockaddrLen) {
    size_t ind = 0;
    size_t lastStart = 0;
    struct message_wrapper* msg;
    while (ind < (length - MESSAGE_WRAPPER_SIZE)) {
        msg = (struct message_wrapper*)(buffer + ind);

        if (msg->start != MESSAGE_WRAPPER_START) {
            // Skip over start byte to repeat
            warn("Invalid packet wrapper start");
            ind++;
            continue;
        }

        lastStart = ind;

        // Handle each message
        int err = ST_FAIL;
        switch (msg->id) {
            case HANDSHAKE_REQUEST:
                err = handle_handshake(server, msg->data, msg->length, recvAddress, sockaddrLen);
                break;
            case CALL_REQUEST:
                err = handle_call_request(server, msg->data, msg->length);
                break;
            case INCOMING_RESPONSE:
                err = handle_incoming_response(server, msg->data, msg->length);
                break;
            case TERMINATE_CALL:
                err = handle_terminate(server, msg->data, msg->length);
                break;
            case TERMINATE_ACK:
                err = handle_terminate_ack(server, msg->data, msg->length);
                break;
            default:
                warn("Unrecognised message id: %x", msg->id);
                break;
        }

        // If handling was a success, then skip over the entire message,
        // else just skip the start byte and start again
        if (err == ST_GOOD) {
            ind += MESSAGE_WRAPPER_SIZE + msg->length;
        } else {
            warn("Failed to handle message with id: %x", msg->id);
            ind ++;
        }
    }

    // We want to return last start here so that a half-read message will 
    // be intact
    *read = lastStart;
    return ST_GOOD;
}

static int handle_handshake(server_t* server, uint8_t* buffer, uint8_t len, struct sockaddr* recvAddress, socklen_t sockaddrLen) {
    info("Handling handshake request");
    if (len < sizeof(struct handshake_request)) {
        return ST_FAIL;
    }

    struct handshake_request* msg = (struct handshake_request*)buffer;

    // Verify magic
    if (strncmp(msg->magic, HANDSHAKE_MAGIC, HANDSHAKE_MAGIC_SIZE) != 0) {
        return ST_FAIL;
    }

    // Allocate number
    uint16_t phoneNumber = allocate_phone_number(server, ntohs(msg->phone_number));

    // Add number to phone list
    if (server->phone_count >= sizeof(server->phone_numbers) / sizeof(*server->phone_numbers)) {
        server->phone_count--;
    }
    
    server->phone_numbers[server->phone_count++] = phoneNumber;

    // Send a response back
    uint8_t response[MESSAGE_WRAPPER_SIZE + sizeof(struct handshake_response)];
    struct message_wrapper* respMsg = (struct message_wrapper*)response;

    respMsg->start = MESSAGE_WRAPPER_START;
    respMsg->id = HANDSHAKE_RESPONSE;
    respMsg->length = sizeof(struct handshake_response);

    struct handshake_response* respData = (struct handshake_response*)respMsg->data;
    strncpy(respData->magic, HANDSHAKE_MAGIC, sizeof(HANDSHAKE_MAGIC));
    respData->phone_number = htons(phoneNumber);

    // TODO Make this non blocking?
    ssize_t sendBytes = sendto(server->sockfd, response, sizeof(response), 0, recvAddress, sockaddrLen);

    if (sendBytes == -1) {
        stl_error(errno, "Failed to respond to handshake");
    }

    if (sendBytes != sizeof(response)) {
        error("Failed to send full handshake packet");
    }

    return ST_GOOD;
}

static int handle_call_request(server_t* server, uint8_t* buffer, uint8_t len) {
    return ST_FAIL;
}

static int handle_incoming_response(server_t* server, uint8_t* buffer, uint8_t len) {
    return ST_FAIL;
}

static int handle_terminate(server_t* server, uint8_t* buffer, uint8_t len) {
    return ST_FAIL;
}

static int handle_terminate_ack(server_t* server, uint8_t* buffer, uint8_t len) {
    return ST_FAIL;
}

static uint16_t allocate_phone_number(server_t* server, uint16_t requested) {
    // Check if phone number exists
    bool found = false;
    uint16_t largest = 0;
    for (int i = 0; i < server->phone_count; i++) {
        if (requested == server->phone_numbers[i]) {
            found = true;
        }

        if (server->phone_numbers[i] > largest) {
            largest = server->phone_numbers[i];
        }
    }

    // If not found then return requested number
    if (!found) {
        return requested;
    }

    // Else just make a new number
    return largest + 1;
}