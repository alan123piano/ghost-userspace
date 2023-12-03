#include "schedulers/fifo/Metric.h"

namespace ghost
{
    void Metric::updateState(std::string_view _newState)
    {
        TaskState newState = getStateFromString(_newState);

        absl::Time currentTime = absl::Now();
        absl::Duration d = currentTime - stateStarted;
        switch (currentState)
        {
        case TaskState::kBlocked:
            blockTime += d;
            break;
        case TaskState::kRunnable:
            runnableTime += d;
            break;
        case TaskState::kQueued:
            queuedTime += d;
            break;
        case TaskState::kOnCpu:
            onCpuTime += d;
            break;
        case TaskState::kYielding:
            yieldingTime += d;
            break;
        default:
            break;
        }
        currentState = newState;
        if(newState == TaskState::kDied)
            diedAt = currentTime;
    }

    void Metric::printResult(FILE *to)
    {
        absl::FPrintF(to, "=============== Result: tid(%" PRId64 ") ==================\n", gtid.id());
        absl::FPrintF(to, "BlockTime: %" PRId64 "\nRunnableTime: %" PRId64 "\nQueuedTime: %" PRId64 "\nonCpuTime: %" PRId64 "\nyieldingTime: %" PRId64 "\n",
                absl::ToInt64Nanoseconds(blockTime), absl::ToInt64Nanoseconds(runnableTime), absl::ToInt64Nanoseconds(queuedTime),
                absl::ToInt64Nanoseconds(onCpuTime), absl::ToInt64Nanoseconds(yieldingTime));
        absl::FPrintF(to, "CreatedAt: %" PRId64 ", DiedAt: %" PRId64 "\n", absl::ToUnixSeconds(createdAt), absl::ToUnixSeconds(diedAt));
        absl::FPrintF(to, "---------------------------------\n");
    }

    Metric::TaskState Metric::getStateFromString(std::string_view state)
    {
        if (state == "Blocked")
            return TaskState::kBlocked;
        else if (state == "Runnable")
            return TaskState::kRunnable;
        else if (state == "Queued")
            return TaskState::kQueued;
        else if (state == "OnCpu")
            return TaskState::kOnCpu;
        else if (state == "Yielding")
            return TaskState::kYielding;
        else if (state == "Died")
            return TaskState::kDied;
        else
        {
            fprintf(stderr, "Task state is unknown(%s)\n", state.data());
            return TaskState::unknown;
        }
    }

    void Metric::sendMessageToOrca()
    {
        orca::OrcaMetric msg;
        msg.gtid = gtid.id();
        msg.created_at_us = absl::ToUnixSeconds(createdAt) * 1000;
        msg.block_time_us = absl::ToInt64Microseconds(blockTime);
        msg.runnable_time_us = absl::ToInt64Microseconds(runnableTime);
        msg.queued_time_us = absl::ToInt64Microseconds(queuedTime);
        msg.on_cpu_time_us = absl::ToInt64Microseconds(onCpuTime);
        msg.yielding_time_us = absl::ToInt64Microseconds(yieldingTime);
        msg.died_at_us = absl::ToUnixSeconds(diedAt) * 1000;
        msg.preempt_count = preemptCount;
        
        messenger.sendBytes((const char *)&msg, sizeof(msg));
    }
}