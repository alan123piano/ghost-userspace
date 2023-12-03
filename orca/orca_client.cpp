#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>

#include "helpers.h"
#include "protocol.h"

void send_message(int port, const char *buf, size_t len) {
    const char *hostname = "localhost";

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;

    // resolve host
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        panic("gethostbyname");
    }
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
    addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        panic("connect");
    }

    // send message
    send_full(sockfd, buf, len);

    // wait for ack
    printf("awaiting ack...\n");
    orca::OrcaHeader header;
    recv_full(sockfd, (char *)&header, sizeof(header));
    if (header.type != orca::MessageType::Ack) {
        panic("expected ack");
    }
    printf("got ack\n");

    close(sockfd);
}

void print_usage() {
    printf("Orca client usage:\n");
    printf("setsched <dfifo|cfifo> <preemption_interval_us=0>\n");
    printf("Press <C-d> to quit.\n");
    std::cout << std::flush;
}

void handle_input(int port, const std::string &input) {
    printf("Running command: %s\n", input.c_str());

    std::istringstream iss(input);

    std::string cmd;
    iss >> cmd;

    if (cmd == "setsched") {
        std::string sched_type;
        int preemption_interval_us = 0;
        iss >> sched_type;
        if (!(iss >> preemption_interval_us)) {
            preemption_interval_us = 0;
        }

        orca::SchedulerConfig config;

        if (sched_type[0] == 'd') {
            config.type = orca::SchedulerConfig::SchedulerType::dFCFS;
        } else if (sched_type[0] == 'c') {
            config.type = orca::SchedulerConfig::SchedulerType::cFCFS;
        } else {
            panic("unrecognized scheduler type");
        }

        if (preemption_interval_us != 0) {
            config.preemption_interval_us = preemption_interval_us;
        }

        orca::OrcaSetScheduler msg;
        msg.config = config;

        send_message(port, (const char *)&msg, sizeof(msg));
    } else {
        printf("Invalid command: %s\n", input.c_str());
        print_usage();
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::ostringstream oss;
        for (int i = 2; i < argc; ++i) {
            oss << argv[i] << " ";
        }
        handle_input(orca::PORT, oss.str());
        return 0;
    }

    print_usage();

    std::string input;
    while (std::getline(std::cin, input)) {
        handle_input(orca::PORT, input);
    }
}
