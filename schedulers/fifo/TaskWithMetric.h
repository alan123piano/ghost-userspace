#pragma once

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/strings/str_format.h"
#include "lib/scheduler.h"

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

            // Send results to Orca
            void sendMessageToOrca();
        };

        Metric m;
        void updateState(std::string_view _newState);

        // WIP
        void updateRuntime();
        void updateTaskRuntime(absl::Duration new_runtime, bool update_elapsed_runtime);

    private:
        // An abstraction for a UDP socket which allows sending messages to Orca
        class OrcaMessenger
        {
        public:
            OrcaMessenger()
            {
                sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                if (sockfd == -1) {
                    panic("error with socket");
                }

                memset(&serverAddr, 0, sizeof(serverAddr));
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(orca::PORT);
                struct hostent* sp = gethostbyname("localhost");
                memcpy(&serverAddr.sin_addr, sp->h_addr_list[0], sp->h_length);
            }

            ~OrcaMessenger()
            {
                close(sockfd);
            }

            // Send bytes to Orca.
            void sendBytes(const char* buf, size_t len)
            {
                sendto(sockfd, buf, len, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            }

        private:
            int sockfd;
            struct sockaddr_in serverAddr;
        };

        OrcaMessenger messenger;
    };
}