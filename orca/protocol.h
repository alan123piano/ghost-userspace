#pragma once

#include "orca.h"

namespace orca {

constexpr int PORT = 8000;
constexpr size_t MAX_MESSAGE_SIZE = 1024;

enum class MessageType { Ack, SetScheduler, Metric };

// Header for all TCP messages to the Orca server.
struct OrcaHeader {
    MessageType type;

    OrcaHeader() {}
    OrcaHeader(MessageType type) : type(type) {}
};

// Message which results in the scheduler being restarted.
struct OrcaSetScheduler : OrcaHeader {
    SchedulerConfig config;

    OrcaSetScheduler() : OrcaHeader(MessageType::SetScheduler) {}
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

} // namespace orca