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
#include "server/upd_forward.h"
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
static int handle_call_request(server_t* server, uint8_t* buffer, uint8_t len, struct sockaddr* recvAddress, socklen_t sockaddrLen);
static int handle_incoming_response(server_t* server, uint8_t* buffer, uint8_t len);
static int handle_terminate(server_t* server, uint8_t* buffer, uint8_t len);

// Misc
static uint16_t allocate_phone_number(server_t* server, uint16_t requested);
static uint16_t allocate_udp_port(server_t* server);

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
    
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_port = htons(server->conf->server_port);
    server->server_addr_len = sizeof(server->server_addr);

    inet_pton(AF_INET, LOCAL_ADDR, &server->server_addr.sin_addr);

    int sockfd = socket(
        AF_INET, 
        SOCK_STREAM,
        0);

    // Check for valid socket id
    if (sockfd < 0) {
        stl_warn(errno, "Failed to initialise socket with code : %d", sockfd);
        close(sockfd);
        return ST_FAIL;
    }

    // Bind socket to local port
    if ((err = bind(sockfd, (const struct sockaddr*)&server->server_addr, server->server_addr_len)) != 0) {
        stl_warn(errno, "Failed to bind address to socket");
        close(sockfd);
        return ST_FAIL;
    }

    info("Server opened on sockfd %d", sockfd);

    server->sockfd = sockfd;
    server->client_count = 0;
    server->ongoing_count = 0;
    server->pending_count = 0;

    if ((err = init_udp_server(&server->udp_server)) != ST_GOOD) {
        warn("Failed to initialise udp server");
        close(sockfd);
        return ST_FAIL;
    }

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

    int connectionfd = 0;

    int error = listen(server->sockfd, 128);

    if (error == -1) {
        stl_warn(errno, "Failed to listen to the socket");
        return ST_FAIL;
    }
    
    while (1) {
        // Read bytes from the socket if not in an overflow state
        socklen_t addressLen = sizeof(recvAddress);

        if (!overflow) {
            connectionfd = accept(server->sockfd, (struct sockaddr*)&server->server_addr, &server->server_addr_len);
            
            if (connectionfd == -1) {
                stl_warn(errno, "Failed to accept connection on socket %d", server->sockfd);
                continue;
            }
            
            info("Accepted a connection");
        }

        ssize_t received = recvfrom(connectionfd, msgBuffer + writeInd, MSG_BUFFER_SIZE - writeInd, 0, (struct sockaddr*)&recvAddress, &addressLen);

        writeInd = 0;

        // Check for an error, TODO probably dont error out here
        if (received == -1) {
            stl_error(errno, "Server failed to receive data");
        }

        // Check to see if the buffer has 'overflowed'
        overflow = received == MSG_BUFFER_SIZE;

        // Iterate over the message buffer
        handle_message_data(server, msgBuffer, received, &writeInd, (struct sockaddr*)&recvAddress, addressLen);

        // If buffer overflow, then shift remaining bytes down
        if (overflow && writeInd < received) {
            memmove(msgBuffer, msgBuffer + writeInd, received - writeInd);
        }
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
                err = handle_call_request(server, msg->data, msg->length, recvAddress, sockaddrLen);
                break;
            case INCOMING_RESPONSE:
                err = handle_incoming_response(server, msg->data, msg->length);
                break;
            case CLIENT_TERMINATE_CALL:
                err = handle_terminate(server, msg->data, msg->length);
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

    // Add to client list
    if (server->client_count >= sizeof(server->clients) / sizeof(*server->clients)) {
        server->client_count--;
    }
    
    client_info_t* clientInfo = &server->clients[server->client_count++];
    clientInfo->phone_number = phoneNumber;
    memcpy(&clientInfo->address, recvAddress, sockaddrLen);
    clientInfo->addrLen = sockaddrLen;

    // Send a response back
    uint8_t response[MESSAGE_WRAPPER_SIZE + sizeof(struct handshake_response)];
    struct message_wrapper* respMsg = (struct message_wrapper*)response;

    respMsg->start = MESSAGE_WRAPPER_START;
    respMsg->id = HANDSHAKE_RESPONSE;
    respMsg->length = sizeof(struct handshake_response);

    struct handshake_response* respData = (struct handshake_response*)respMsg->data;
    strncpy(respData->magic, HANDSHAKE_MAGIC, sizeof(HANDSHAKE_MAGIC));
    respData->phone_number = htons(phoneNumber);

    ssize_t sendBytes = sendto(server->sockfd, response, sizeof(response), 0, recvAddress, sockaddrLen);

    if (sendBytes == -1) {
        stl_error(errno, "Failed to respond to handshake");
    }

    if (sendBytes != sizeof(response)) {
        error("Failed to send full handshake packet");
    }

    info("Handshake successful with number: %hu", phoneNumber);

    return ST_GOOD;
}

static int handle_call_request(server_t* server, uint8_t* buffer, uint8_t len, struct sockaddr* recvAddress, socklen_t sockaddrLen) {
    info("Handling call request");
    if (len != sizeof(struct call_request)) {
        warn("Invalid message size for call request");
        return ST_FAIL;
    }

    struct call_request* callRequest = (struct call_request*)buffer;

    const uint16_t fromPhoneNumber = ntohs(callRequest->from_phone_number);
    const uint16_t toPhoneNumber = ntohs(callRequest->to_phone_number);

    // First lookup the phone number and check that it is an 'online' number
    client_info_t* fromClient = NULL;
    client_info_t* toClient = NULL;

    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].phone_number == fromPhoneNumber) {
            info("From phone number found");
            fromClient = &server->clients[i];
        }
        if (server->clients[i].phone_number == toPhoneNumber) {
            info("To phone number found");
            toClient = &server->clients[i];
        }
    }

    if (fromClient == NULL || toClient == NULL) {
        info("To / from phone number not valid");
        return ST_GOOD;
    }

    // Start the udp server
    uint16_t updPort = allocate_udp_port(server);
    start_udp_port(&server->udp_server, updPort);

    // Add to pending calls list
    call_info_t* pendingCall = &server->pending_calls[server->pending_count++];
    pendingCall->caller = fromPhoneNumber;
    pendingCall->callee = toPhoneNumber;
    pendingCall->port = updPort;

    // Respond to caller
    uint8_t callRespBuf[MESSAGE_WRAPPER_SIZE + sizeof(struct call_response)];
    struct message_wrapper* wrapper = (struct message_wrapper*)callRespBuf;
    
    wrapper->start = MESSAGE_WRAPPER_START;
    wrapper->id = CALL_RESPONSE;
    wrapper->length = sizeof(struct call_response);
    
    struct call_response* respMsg = (struct call_response*)wrapper->data;
    respMsg->udp_server_port = htons(updPort);
    
    ssize_t bytesSent = sendto(server->sockfd, callRespBuf, sizeof(callRespBuf), 0, (struct sockaddr*)&fromClient->address, fromClient->addrLen);
    (void) bytesSent;

    // Call the other number
    uint8_t incomingCallBuf[MESSAGE_WRAPPER_SIZE + sizeof(struct incoming_call)];
    wrapper = (struct message_wrapper*)incomingCallBuf;
    
    wrapper->start = MESSAGE_WRAPPER_START;
    wrapper->id = INCOMING_CALL;
    wrapper->length = sizeof(struct incoming_call);
    
    struct incoming_call* incomMsg = (struct incoming_call*)wrapper->data;
    incomMsg->from_phone_number = htons(fromPhoneNumber);
    incomMsg->udp_server_port = htons(updPort);
    
    bytesSent = sendto(server->sockfd, incomingCallBuf, sizeof(incomingCallBuf), 0, (struct sockaddr*)&toClient->address, toClient->addrLen);
    (void) bytesSent;
    
    return ST_GOOD;
}

static int handle_incoming_response(server_t* server, uint8_t* buffer, uint8_t len) {
    info("Handling incoming call response");
    if (len != sizeof(struct incoming_response)) {
        warn("Invalid message size for incoming response");
        return ST_FAIL;
    }

    struct incoming_response* response = (struct incoming_response*)buffer;

    uint16_t phoneNumber = ntohs(response->from_phone_number);

    // Find in pending calls
    int index = -1;
    for (int i = 0; i < server->pending_count; i++) {
        if (server->pending_calls[i].callee == phoneNumber) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        warn("Incoming response callee not found");
        return ST_FAIL;
    }

    call_info_t* pendingCall = &server->pending_calls[index];

    // Success so first add to ongoing calls
    call_info_t* ongoingCall = &server->ongoing_calls[server->ongoing_count++];
    memcpy(ongoingCall, pendingCall, sizeof(call_info_t));

    // Now remove pending call
    if (index != server->pending_count - 1) {
        // Swap pending call with the last one to 'remove' it
        memcpy(pendingCall, &server->pending_calls[server->pending_count - 1], sizeof(call_info_t));
    }
    server->pending_count--;

    return ST_GOOD;
}

static int handle_terminate(server_t* server, uint8_t* buffer, uint8_t len) {
    info("Handling call terminate");
    if (len != sizeof(struct client_terminate_call)) {
        warn("Invalid message size for terminate call");
        return ST_FAIL;
    }

    struct client_terminate_call* clientTerm = (struct client_terminate_call*)buffer;

    info("Terminating call with code: %hu", clientTerm->err_code);

    uint16_t phoneNumber = ntohs(clientTerm->phone_number);

    // Terminate the call
    call_info_t* callInfo = NULL;
    bool ongoing = false;
    for (int i = 0; i < server->ongoing_count; i++) {
        if (server->ongoing_calls[i].callee == phoneNumber || server->ongoing_calls[i].caller == phoneNumber) {
            callInfo = server->ongoing_calls + i;
            ongoing = true;
            break;
        }
    }

    // Check pending calls if not found
    if (callInfo == NULL) {
        for (int i = 0; i < server->pending_count; i++) {
            if (server->pending_calls[i].callee == phoneNumber || server->pending_calls[i].caller == phoneNumber) {
                callInfo = server->pending_calls + i;
                break;
            }
        }
    }

    // Send terminate call to other client
    uint16_t toTerminate = callInfo->callee == phoneNumber ? callInfo->caller : callInfo->callee;

    // Find the client
    client_info_t* client = NULL;
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i].phone_number == toTerminate) {
            client = server->clients + i;
            break;
        }
    }

    if (client == NULL) {
        warn("Erm what the sigma");
        return ST_FAIL;
    }

    info("Terminating call for phone number: %hu, from %hu", toTerminate, phoneNumber);

    uint8_t msgBuffer[MESSAGE_WRAPPER_SIZE + sizeof(struct terminate_call)];

    struct message_wrapper* wrapper = (struct message_wrapper*)msgBuffer;
    wrapper->start = MESSAGE_WRAPPER_START;
    wrapper->id = TERMINATE_CALL;
    wrapper->length = sizeof(struct terminate_call);

    struct terminate_call* termCall = (struct terminate_call*)wrapper->data;
    termCall->err_code = CALL_PUTDOWN;

    ssize_t bytesSent = sendto(server->sockfd, msgBuffer, sizeof(msgBuffer), 0, (struct sockaddr*)&client->address, client->addrLen);

    if (bytesSent == -1) {
        warn("Failed to send terminate to client");
        return ST_FAIL;
    }

    if (bytesSent != sizeof(msgBuffer)) {
        warn("Full terminate request not sent");
        return ST_FAIL;
    }

    // Kill ongoing call
    if (ongoing) {
        // First stop the udp port
        stop_udp_port(&server->udp_server, callInfo->port);
    }

    // Now destroy the struct
    call_info_t* callArray = ongoing ? server->ongoing_calls : server->pending_calls;
    int* count = ongoing ? &server->ongoing_count : &server->pending_count;

    if (callInfo - callArray < *count - 1) {
        memcpy(callInfo, callArray + *count - 1, sizeof(call_info_t));
    }

    (*count)--;

    return ST_GOOD;
}

static uint16_t allocate_phone_number(server_t* server, uint16_t requested) {
    // Check if phone number exists
    bool found = false;
    uint16_t largest = 0;
    for (int i = 0; i < server->client_count; i++) {
        if (requested == server->clients[i].phone_number) {
            found = true;
        }

        if (server->clients[i].phone_number > largest) {
            largest = server->clients[i].phone_number;
        }
    }

    // If not found then return requested number
    if (!found) {
        return requested;
    }

    // Else just make a new number
    return largest + 1;
}

static uint16_t allocate_udp_port(server_t* server) {
    return 9090;
}