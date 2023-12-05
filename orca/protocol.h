#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orca.h"

namespace orca {

constexpr int PORT = 8000;
constexpr size_t MAX_MESSAGE_SIZE = 1024;

enum class MessageType {
    Ack,
    SetScheduler,
    DetermineScheduler,
    Metric,
    IngressHint
};

// Header for all TCP messages to the Orca server.
struct OrcaHeader {
    MessageType type;

    OrcaHeader() {}
    OrcaHeader(MessageType type) : type(type) {}
};

struct OrcaAck : OrcaHeader {
    // include optional data with ack (debug)
    char data[100];

    OrcaAck() : OrcaHeader(MessageType::Ack) { memset(data, 0, sizeof(data)); }
};

// Message which results in the scheduler being restarted.
struct OrcaSetScheduler : OrcaHeader {
    SchedulerConfig config;

    OrcaSetScheduler() : OrcaHeader(MessageType::SetScheduler) {}
};

// Triggers a scheduler update based on Orca's best guess.
struct OrcaDetermineScheduler : OrcaHeader {
    OrcaDetermineScheduler() : OrcaHeader(MessageType::DetermineScheduler) {}
};

// Payload from a Metrics object
struct OrcaMetric : OrcaHeader {
    // Gtid (raw int64 value)
    int64_t gtid;

    // Timestamps for various metrics
    int64_t created_at_us;
    int64_t block_time_us;
    int64_t runnable_time_us;
    int64_t queued_time_us;
    int64_t on_cpu_time_us;
    int64_t yielding_time_us;
    int64_t died_at_us;

    // Number of preemptions
    int64_t preempt_count;

    OrcaMetric() : OrcaHeader(MessageType::Metric) {}
};

// Hints about runtime for ingress requests
struct OrcaIngressHint : OrcaHeader {
    enum class ReqLength {
        Short, // ex. RocksDB Get
        Long,  // ex. RocksDB Range
    };

    // A hint which tells Orca about an incoming request type.
    ReqLength hint;

    OrcaIngressHint() : OrcaHeader(MessageType::IngressHint) {}
};

// class which allows convenient sending of UDP messages to Orca
class OrcaUDPClient {
public:
    OrcaUDPClient() {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1) {
            panic("error with socket");
        }

        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(orca::PORT);
        struct hostent *sp = gethostbyname("localhost");
        memcpy(&serverAddr.sin_addr, sp->h_addr_list[0], sp->h_length);
    }

    ~OrcaUDPClient() { close(sockfd); }

    // Send bytes to Orca.
    void send_bytes(const char *buf, size_t len) {
        sendto(sockfd, buf, len, 0, (sockaddr *)&serverAddr,
               sizeof(serverAddr));
    }

private:
    int sockfd;
    struct sockaddr_in serverAddr;
};

} // namespace orca