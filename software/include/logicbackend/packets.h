#ifndef SRC_PACKETS_H
#define SRC_PACKETS_H

#include <stdint.h>

#define HANDSHAKE_MAGIC "bro"

#define PACKED_STRUCT __attribute((packed))

enum MSG_ID {
    HANDSHAKE_REQUEST   = 1,
    HANDSHAKE_RESPONSE  = 2,
    CALL_REQUEST        = 3,
    CALL_RESPONSE       = 4,
    INCOMING_CALL       = 5,
    INCOMING_RESPONSE   = 6,
};

enum CALL_ENUM {
    CALL_REJECTED = 0,
    CALL_ACCEPTED = 1,
};

/**
 * Sent by a node to the server to request a handshake.
 */
struct handshake_request {
    uint8_t id;
    char magic[4];
} PACKED_STRUCT;

/**
 * Sent by the server to a node to accept a handshake.
 */
struct handshake_response {
    uint8_t id;
    char magic[4];
} PACKED_STRUCT;

/**
 * Sent by a client to the server to request a call.
 */
struct call_request {
    uint8_t id;
    uint16_t phone_number;
} PACKED_STRUCT;

/**
 * Sent by the server to the client to indicate whether their call request 
 * has been accepted or not.
 */
struct call_response {
    uint8_t id;
    uint8_t accepted; // enum CALL_ENUM
    uint16_t udp_server_port;
} PACKED_STRUCT;

/**
 * Sent by server to client to signal an incoming call.
 * 
 * Specifies the server's upd port onto which the client should listen
 * for incoming audio data, and write its own audio data.
 */
struct incoming_call {
    uint8_t id;
    uint16_t from_phone_number;
    uint16_t udp_server_port;
} PACKED_STRUCT;

/**
 * Sent by the client to the server to represent a response to an incoming 
 * call.
 */
struct incoming_response {
    uint8_t id;
    uint8_t accepted; // enum CALL_ENUM
};

#endif