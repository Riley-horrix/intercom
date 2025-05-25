#ifndef SRC_PACKETS_H
#define SRC_PACKETS_H

#include <stdint.h>

#define HANDSHAKE_MAGIC "bro"
#define HANDSHAKE_MAGIC_SIZE sizeof(HANDSHAKE_MAGIC)

#define PACKED_STRUCT __attribute((packed))

enum MSG_ID {
    HANDSHAKE_REQUEST   = 1,
    HANDSHAKE_RESPONSE  = 2,
    CALL_REQUEST        = 10,
    CALL_RESPONSE       = 11,
    INCOMING_CALL       = 12,
    INCOMING_RESPONSE   = 13,
    TERMINATE_CALL      = 20,
    TERMINATE_ACK       = 21,
};

enum TERMINATE_CODE {
    CALL_PUTDOWN = 1,
    SERVER_ERROR = 2,
};

#define MESSAGE_WRAPPER_START ((uint8_t)0xAA)
#define MESSAGE_WRAPPER_SIZE sizeof(struct message_wrapper)

/**
 * Message wrapper for all server - client communication
 */
struct message_wrapper {
    uint8_t start;  // 0xAA
    uint8_t length; // 0-256 bytes
    uint8_t id;
    uint8_t data[];
};

/**
 * Sent by a node to the server to request a handshake.
 * 
 * Phone number represents the node's preferred phone number.
 */
struct handshake_request {
    uint16_t phone_number;
    char magic[4];
} PACKED_STRUCT;

/**
 * Sent by the server to a node to accept a handshake.
 * 
 * Returns the clients allocated number.
 */
struct handshake_response {
    uint16_t phone_number;
    char magic[4];
} PACKED_STRUCT;

/**
 * Sent by a client to the server to request a call.
 * 
 * The server can send back a call response for a successful call, or a 
 * terminate call for unsuccessful.
 */
struct call_request {
    uint16_t phone_number;
} PACKED_STRUCT;

/**
 * Sent by the server to the client to indicate whether their call request 
 * has been accepted or not.
 */
struct call_response {
    uint16_t udp_server_port;
} PACKED_STRUCT;

/**
 * Sent by server to client to signal an incoming call.
 * 
 * Specifies the server's upd port onto which the client should listen
 * for incoming audio data, and write its own audio data.
 * 
 * A client should reply with an incoming_response message to accept, or a 
 * terminate_call message to reject.
 */
struct incoming_call {
    uint16_t from_phone_number;
    uint16_t udp_server_port;
} PACKED_STRUCT;

/**
 * Sent by the client to the server to represent accepting an incoming phone 
 * call.
 */
struct incoming_response {
    uint16_t from_phone_number;
} PACKED_STRUCT;

/**
 * Sent by server to client or client to server to represent an end of call.
 */
struct terminate_call {
    uint8_t err_code;
} PACKED_STRUCT;


void* receive_wrapped_message(void* msg, size_t msgLen, size_t desiredLen, uint8_t msgId);

#endif