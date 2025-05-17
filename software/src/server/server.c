#include "args.h"
#include "common.h"
#include "server/server.h"

// To run the server needs : TCP port, UDP port min, UPD port max

// When a new connection arrives, fork to handle. This child process
// will 'own' the connection with that node. When a call happens the
// child will fork again and start its child to run the udp forwarding

static int init_socket(server_conf_t* config, int* sockfd);

int server_run(int argc, char** argv) {
    server_conf_t config;
    info("Initialising server configuration");
    init_server_conf(&config, argc, argv);

    int sockfd;
    int err = init_socket(&config, &sockfd);

    return err == ST_GOOD ? 0 : 1;
}

static int init_socket(server_conf_t* config, int* sockfd) {
    return ST_GOOD;
}