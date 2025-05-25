#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "server/upd_forward.h"

static void udp_server_main(udp_port_info_t* portInfo);

/**
 * Initialises the udp socket.
 */
int init_udp_server(udp_server_t* server) {
    server->port_count = 0;
    return ST_GOOD;
}

/**
 * Initialises a chile process that, once started, will listen to the udp port
 * and forward bytes between the senders on the port.
 * 
 * @param port The port to listen to.
 */
int start_udp_port(udp_server_t* server, uint16_t port) {
    udp_port_info_t* pInfo = (udp_port_info_t*)create_shared_memory(sizeof(udp_port_info_t));
    server->ports[server->port_count++] = pInfo;
    
    info("Starting udp port on port: %hu", port);

    pInfo->port = port;

    // Initialise udp socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd == -1) {
        warn("Failed to initialise udp server port");
        return ST_FAIL;
    }

    pInfo->sockfd = sockfd;

    // Initialise process
    pid_t pid = fork();

    if (pid == -1) {
        // Failure to fork
        stl_warn(errno, "Failed to fork to start the udp server");
        return ST_FAIL;
    }

    if (pid != 0) {
        // Parent process
        // Close the socket for the parent process (this may break it?)
        close(sockfd);
        pInfo->pid = pid;
    } else {
        // Child process
        udp_server_main(pInfo);
    }

    return ST_GOOD;
}

int stop_udp_port(udp_server_t* server, uint16_t port) {
    // Find the port info
    for (int i = 0; i < server->port_count; i++) {
        if (server->ports[i]->port == port) {
            // Kill the child
            info("Killing udp server on port: %hu", port);
            kill(server->ports[i]->pid, SIGINT);
            return ST_GOOD;
        }
    }

    warn("Unable to find udp server to kill on port: %hu", port);
    return ST_FAIL;
}

/**
 * The main upd transfer function. This function sends bytes of audio data between clients.
 * 
 * This function returns when the parent kills it.
 */
static void udp_server_main(udp_port_info_t* portInfo) {
    while (1) {

    }
}

