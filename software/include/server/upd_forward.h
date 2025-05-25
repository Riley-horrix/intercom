#ifndef SRC_UDP_FORWARD_H
#define SRC_UDP_FORWARD_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct udp_port_info {
    int sockfd;
    uint16_t port;
    pid_t pid;
} udp_port_info_t;

typedef struct udp_server {
    int port_count;
    udp_port_info_t* ports[10];
} udp_server_t;

int init_udp_server(udp_server_t* server);

int start_udp_port(udp_server_t* server, uint16_t port);
int stop_udp_port(udp_server_t* server, uint16_t port);

#endif