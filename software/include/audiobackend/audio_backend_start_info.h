#ifndef SRC_AUDIO_BACKEND_START_INFO_H
#define SRC_AUDIO_BACKEND_START_INFO_H

#define IPV4_MAX_STRLEN 16

struct audio_backend_start_info {
    unsigned short sendPort;
    unsigned short recvPort;
    char sendAddr[IPV4_MAX_STRLEN];
    char recvAddr[IPV4_MAX_STRLEN];
};

#endif