#ifndef GHOST_PROFILER_H
#define GHOST_PROFILER_H

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/strings/str_format.h"
#include "lib/scheduler.h"

#include <inttypes.h>
#include <stdio.h>

namespace ghost
{
    struct TaskWithMetric : public Task<>
    {
    private:
        enum class TaskState
        {
            kCreated,
            kBlocked,
            kRunnable,
            kQueued,
            kOnCpu,
            kYielding,
            kDied,
            unknown
        };
        static TaskState getStateFromString(std::string_view);

    public:
        TaskWithMetric(Gtid gtid, ghost_sw_info sw_info)
            : Task<>(gtid, sw_info), m(gtid) {}

        struct Metric // Record how long it stayed in that state
        {
        public:
            Gtid gtid;

            absl::Time createdAt;        // created time
            absl::Duration blockTime;    // Blocked state
            absl::Duration runnableTime; // runnable state
            absl::Duration queuedTime;   // Queued state
            absl::Duration onCpuTime;    // OnCPU state
            absl::Duration yieldingTime;

            // Cumulative runtime in ns.
            absl::Duration runtime;
            // Accrued CPU time in ns.
            absl::Duration elapsedRuntime;

            absl::Time diedAt;
            int64_t preemptCount; // if it's preempted

            TaskState currentState;
            absl::Time stateStarted;

            Metric() : blockTime(absl::ZeroDuration()), runnableTime(absl::ZeroDuration()),
                       queuedTime(absl::ZeroDuration()), onCpuTime(absl::ZeroDuration()), yieldingTime(absl::ZeroDuration()),
                       runtime(absl::ZeroDuration()), elapsedRuntime(absl::ZeroDuration()) {}

            Metric(Gtid _gtid) : gtid(_gtid), createdAt(absl::Now()), blockTime(absl::ZeroDuration()), runnableTime(absl::ZeroDuration()),
                                 queuedTime(absl::ZeroDuration()), onCpuTime(absl::ZeroDuration()), yieldingTime(absl::ZeroDuration()),
                                 runtime(absl::ZeroDuration()), elapsedRuntime(absl::ZeroDuration()),
                                 currentState(TaskState::kCreated), stateStarted(createdAt)
            {
            }
            void printResult(FILE *to);
            static double stddev(const std::vector<Metric> &v);

        private:
            static Metric::TaskState getStateFromString(std::string_view state);
        };

        Metric m;
        void updateState(std::string_view _newState);
        // void updateRuntime(bool updateElapsedRuntime); // WIP
    };
}
#endif