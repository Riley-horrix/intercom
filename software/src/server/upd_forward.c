#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
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
    // List of currently connected addresses
    info("Child started udp audio server on socket %d", portInfo->sockfd);
    bool init[2];
    struct sockaddr_in addrs[2];
    socklen_t addrLens[2];

    // Temporary buffer
    uint8_t msgBuffer[BIT(12)];

    while (1) {
        struct sockaddr_in addr;
        socklen_t addrLen;

        ssize_t bytesRead = recvfrom(portInfo->sockfd, msgBuffer, sizeof(msgBuffer), 0, (struct sockaddr*)&addr, &addrLen);

        if (bytesRead == -1) {
            stl_warn(errno, "Failed to read audio data from client");
            continue;
        }

        // Add received from client to array if not there
        // This loops until both clients connected
        if (!init[0] || !init[1]) {
            int toAdd = init[0] ? 1 : 0;
            int other = toAdd ^ 0x1;
            
            // Check not already added with same address
            if (memcmp(&addr.sin_addr, &addrs[other].sin_addr, sizeof(addr.sin_addr)) != 0) {
                memcpy(&addrs[toAdd], &addr, addrLen);
                addrLens[toAdd] = addrLen;
                init[toAdd] = true;

                char addrBuf[INET_ADDRSTRLEN];
                const char* str = inet_ntop(AF_INET, &addr.sin_addr, addrBuf, INET_ADDRSTRLEN);

                if (str == NULL) {
                    warn("Issue translating added IPV4 address");
                } else {
                    info("UPD audio thread added address %s", addrBuf);
                }
            }

            continue;
        }

        // Send to other client
        int receiver = memcmp(&addr.sin_addr, &addrs[0].sin_addr, sizeof(addr.sin_addr)) == 0 ? 1 : 0;

        ssize_t bytesSent = sendto(portInfo->sockfd, msgBuffer, bytesRead, 0, (struct sockaddr*)&addrs[receiver], addrLens[receiver]);

        if (bytesSent == -1) {
            stl_warn(errno, "Failed to send audio data to client");
            continue;
        }
    }
}

