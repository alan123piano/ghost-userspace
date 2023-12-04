#include "orca_messenger.h"

void OrcaMessenger::sendMessageToOrca(const ghost::TaskWithMetric::Metric &m)
{
    orca::OrcaMetric msg;
    msg.gtid = m.gtid.id();
    msg.created_at_us = absl::ToUnixMicros(m.createdAt);
    msg.block_time_us = absl::ToInt64Microseconds(m.blockTime);
    msg.runnable_time_us = absl::ToInt64Microseconds(m.runnableTime);
    msg.queued_time_us = absl::ToInt64Microseconds(m.queuedTime);
    msg.on_cpu_time_us = absl::ToInt64Microseconds(m.onCpuTime);
    msg.yielding_time_us = absl::ToInt64Microseconds(m.yieldingTime);
    msg.died_at_us = absl::ToUnixMicros(m.diedAt);
    msg.preempt_count = m.preemptCount;

    messenger.sendBytes((const char *)&msg, sizeof(msg));
}