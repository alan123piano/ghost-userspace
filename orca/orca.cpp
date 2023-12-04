#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "event_signal.h"
#include "helpers.h"
#include "orca.h"
#include "protocol.h"

int main(int argc, char *argv[]) {
    // TCP socket
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpfd == -1) {
        panic("tcp socket");
    }

    // UDP socket
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd == -1) {
        panic("udp socket");
    }

    int yesval = 1;
    if (setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) ==
        -1) {
        panic("setsockopt");
    }

    // set up our sockaddr
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(orca::PORT);

    // bind TCP
    if (bind(tcpfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        panic("bind tcp");
    }

    // bind UDP
    if (bind(udpfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
        panic("bind udp");
    }

    // listen TCP
    if (listen(tcpfd, 10) == -1) {
        panic("listen");
    }

    // put orca_agent ptr in static memory (so SIGINT handler can clean it up)
    static std::unique_ptr<orca::Orca> orca_agent;
    orca_agent = std::make_unique<orca::Orca>();

    // run dFCFS by default
    orca_agent->set_scheduler(orca::SchedulerConfig{
        .type = orca::SchedulerConfig::SchedulerType::dFCFS});

    signal(SIGINT, [](int signum) {
        // call Orca destructor
        orca_agent = nullptr;

        exit(signum);
    });

    EventSignal<int> sched_ready;

    printf("Orca listening on port %d...\n", orca::PORT);
    while (true) {
        int sched_stdout = orca_agent->get_sched_stdout_fd();
        int sched_stderr = orca_agent->get_sched_stderr_fd();

        // set up fd set for select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(tcpfd, &readfds);
        FD_SET(udpfd, &readfds);
        if (sched_stdout != -1) {
            FD_SET(sched_stdout, &readfds);
        }
        if (sched_stderr != -1) {
            FD_SET(sched_stderr, &readfds);
        }

        // set timeout of 1ms
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        int ready = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

        if (ready == -1) {
            panic("select");
        } else if (ready == 0) {
            // timeout occurred, continue loop
            continue;
        } else {
            if (FD_ISSET(tcpfd, &readfds)) {
                int connfd = accept(tcpfd, NULL, NULL);
                if (connfd == -1) {
                    printf("accept returned -1\n");
                    continue;
                }

                char buf[orca::MAX_MESSAGE_SIZE];
                memset(buf, 0, sizeof(buf));

                recv_full(connfd, buf, sizeof(orca::OrcaHeader));
                auto *header = (orca::OrcaHeader *)buf;

                switch (header->type) {
                case orca::MessageType::SetScheduler: {
                    recv_full(connfd, buf + sizeof(orca::OrcaHeader),
                              sizeof(orca::OrcaSetScheduler) -
                                  sizeof(orca::OrcaHeader));

                    auto *msg = (orca::OrcaSetScheduler *)buf;
                    std::cout
                        << "Received SetScheduler. type="
                        << (int)msg->config.type << ", preemption_interval_us="
                        << msg->config.preemption_interval_us << std::endl;

                    orca_agent->set_scheduler(msg->config);

                    sched_ready.once([connfd, &sched_ready](int) {
                        // send ack
                        orca::OrcaHeader ack(orca::MessageType::Ack);
                        send_full(connfd, (const char *)&ack, sizeof(ack));

                        close(connfd);
                    });

                    break;
                }
                default:
                    panic("unimplemented tcp message type");
                }
            }
            if (FD_ISSET(udpfd, &readfds)) {
                char buf[orca::MAX_MESSAGE_SIZE];
                memset(buf, 0, sizeof(buf));

                ssize_t result =
                    recvfrom(udpfd, buf, orca::MAX_MESSAGE_SIZE, 0, NULL, NULL);
                if (result <= 0) {
                    panic("recvfrom");
                }
                if (result < sizeof(orca::OrcaHeader)) {
                    panic("udp datagram: received less than a header");
                }

                auto *header = (orca::OrcaHeader *)buf;

                switch (header->type) {
                case orca::MessageType::Metric: {
                    auto *msg = (orca::OrcaMetric *)buf;
                    std::cout << "Received Metric. gtid=" << msg->gtid
                              << ", created_at_us=" << msg->created_at_us
                              << ", block_time_us=" << msg->block_time_us
                              << ", runnable_time_us=" << msg->runnable_time_us
                              << ", queued_time_us=" << msg->queued_time_us
                              << ", on_cpu_time_us=" << msg->on_cpu_time_us
                              << ", yielding_time_us=" << msg->yielding_time_us
                              << ", died_at_us=" << msg->died_at_us
                              << ", preempt_count=" << msg->preempt_count
                              << std::endl;

                    // TODO: do stuff based on metric

                    break;
                }
                default:
                    printf("Unknown UDP message type: %d\n", (int)header->type);
                    panic("unimplemented udp message type");
                }
            }
            if (sched_stdout != -1 && FD_ISSET(sched_stdout, &readfds)) {
                char buf[8192];
                memset(buf, 0, sizeof(buf));

                if (read(sched_stdout, buf, sizeof(buf) - 1) == -1) {
                    panic("read");
                }

                // forward scheduler's stdout to our stdout
                std::cout << buf << std::flush;

                if (strstr(buf, "Initialization complete, ghOSt active.")) {
                    sched_ready.fire(0);
                }
            }
            if (sched_stderr != -1 && FD_ISSET(sched_stderr, &readfds)) {
                char buf[8192];
                memset(buf, 0, sizeof(buf));

                if (read(sched_stderr, buf, sizeof(buf) - 1) == -1) {
                    panic("read");
                }

                // forward scheduler's stderr to our stderr
                std::cerr << buf << std::flush;

                /**
                 *  If we find a substring indicating crash:
                 *      Call orca_agent->set_scheduler()
                 */
            }
        }
    }
}