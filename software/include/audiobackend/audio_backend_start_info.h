#ifndef SRC_AUDIO_BACKEND_START_INFO_H
#define SRC_AUDIO_BACKEND_START_INFO_H

#include <netinet/in.h>

#define IPV4_MAX_STRLEN 16

typedef struct audio_backend_start_info {
    struct sockaddr_in serverAddr;
    socklen_t serverAddrLen;
} audio_backend_start_info_t;

#endif