#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orca/protocol.h"
#include "orca/helpers.h"
#include "schedulers/fifo/TaskWithMetric.h"

// An abstraction for a UDP socket which allows sending messages to Orca
class OrcaMessenger
{
public:
    OrcaMessenger()
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1)
        {
            panic("error with socket");
        }

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(orca::PORT);
        struct hostent *sp = gethostbyname("localhost");
        memcpy(&serverAddr.sin_addr, sp->h_addr_list[0], sp->h_length);
    }

    ~OrcaMessenger()
    {
        close(sockfd);
    }

    // Send bytes to Orca.
    void sendBytes(const char *buf, size_t len)
    {
        sendto(sockfd, buf, len, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
    }
    void sendMessageToOrca(const ghost::TaskWithMetric::Metric &m);

private:
    int sockfd;
    struct sockaddr_in serverAddr;
};