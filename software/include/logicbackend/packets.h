#ifndef SRC_PACKETS_H
#define SRC_PACKETS_H

#include <stdint.h>

#define HANDSHAKE_MAGIC "bro"

struct handshake_request {
    char magic[4];
};

struct handshake_response {
    char magic[4];
};

#endif