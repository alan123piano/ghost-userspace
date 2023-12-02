#include "schedulers/fifo/TaskWithMetric.h"

#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

namespace ghost
{
    void TaskWithMetric::updateState(std::string_view _newState)
    {
        TaskState newState = getStateFromString(_newState);

        absl::Time currentTime = absl::Now();
        absl::Duration d = currentTime - m.stateStarted;
        switch (m.currentState)
        {
        case TaskState::kBlocked:
            m.blockTime += d;
            break;
        case TaskState::kRunnable:
            m.runnableTime += d;
            break;
        case TaskState::kQueued:
            m.queuedTime += d;
            break;
        case TaskState::kOnCpu:
            m.onCpuTime += d;
            break;
        case TaskState::kYielding:
            m.yieldingTime += d;
            break;
        default:
            break;
        }
        m.currentState = newState;
        m.stateStarted = currentTime;
        if (newState == TaskState::kDied)
            m.diedAt = currentTime;
    }

    // void TaskWithMetric::updateRuntime(bool updateElapsedRuntime)
    // {
    //     absl::Duration new_runtime = absl::Nanoseconds(status_word.runtime());
    //     CHECK_GE(new_runtime, m.runtime);
    //     if (updateElapsedRuntime)
    //     {
    //         m.elapsedRuntime += new_runtime - m.runtime;
    //     }
    //     m.runtime = new_runtime;
    // }

    void TaskWithMetric::Metric::printResult(FILE *to)
    {
        absl::FPrintF(to, "=============== Result: tid(%" PRId64 ") ==================\n", gtid.id());
        absl::FPrintF(to, "BlockTime: %" PRId64 "\nRunnableTime: %" PRId64 "\nQueuedTime: %" PRId64 "\nonCpuTime: %" PRId64 "\nyieldingTime: %" PRId64 "\n",
                      absl::ToInt64Nanoseconds(blockTime), absl::ToInt64Nanoseconds(runnableTime), absl::ToInt64Nanoseconds(queuedTime),
                      absl::ToInt64Nanoseconds(onCpuTime), absl::ToInt64Nanoseconds(yieldingTime));
        absl::FPrintF(to, "CreatedAt: %" PRId64 ", DiedAt: %" PRId64 "\n", absl::ToUnixSeconds(createdAt), absl::ToUnixSeconds(diedAt));
        absl::FPrintF(to, "---------------------------------\n");
    }

    TaskWithMetric::TaskState TaskWithMetric::getStateFromString(std::string_view state)
    {
        if (state == "Blocked")
            return TaskState::kBlocked;
        else if (state == "Runnable")
            return TaskState::kRunnable;
        else if (state == "Queued")
            return TaskState::kQueued;
        else if (state == "OnCpu")
            return TaskState::kOnCpu;
        else if (state == "yielding")
            return TaskState::kYielding;
        else if (state == "Died")
            return TaskState::kDied;
        else
        {
            fprintf(stderr, "Task state is unknown(%s)\n", state.data());
            return TaskState::unknown;
        }
    }

    double TaskWithMetric::Metric::stddev(const std::vector<Metric> &v)
    {
        double sum = 0;
        for (auto &m : v)
        {
            sum += absl::ToDoubleMicroseconds(m.onCpuTime);
        }
        double m = sum / v.size();

        double accum = 0.0;
        std::for_each(std::begin(v), std::end(v), [&](const Metric &d)
                      { double diff = absl::ToDoubleMicroseconds(d.onCpuTime) - m;
                        accum += diff*diff; });

        double stdev = sqrt(accum / (v.size() - 1));
        return stdev;
    }
}