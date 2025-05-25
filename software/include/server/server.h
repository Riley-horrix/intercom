#ifndef SRC_SERVER
#define SRC_SERVER

#include <netinet/in.h>
#include <unistd.h>
#include <stdint.h>
#include "utils/args.h"
#include "server/upd_forward.h"

typedef struct call_info {
    uint64_t time;
    unsigned short port;
    int caller;
    int callee;
    pid_t pid;
} call_info_t;

typedef struct client_info {
    uint16_t phone_number;
    struct sockaddr_in address;
    socklen_t addrLen; 
} client_info_t;

typedef struct server {
    server_conf_t* conf;
    udp_server_t udp_server;
    int sockfd;
    int client_count;
    int pending_count;
    int ongoing_count;
    client_info_t clients[10];
    call_info_t pending_calls[10];
    call_info_t ongoing_calls[10];
} server_t;

extern int server_run(int argc, char** argv);

#endif