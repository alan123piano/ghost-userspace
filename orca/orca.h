#pragma once

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "helpers.h"
#include "protocol.h"

namespace orca {

// Helper which suggests a scheduling config based on input data.
// TODO: this class could form the basis of a more generalized analysis.
class MetricAnalyzer {
public:
    // Indicate that we saw a short request.
    void add_short() { ++num_short; }

    // Indicate that we saw a long request.
    void add_long() { ++num_long; }

    // Add a metric to the analyzer.
    void add_metric(orca::OrcaMetric metric) { metrics.push_back(metric); }

    // Suggest a config based on workload stats
    SchedulerConfig suggest_from_ingress_hints() {
        if (num_short + num_long == 0) {
            // avoid div by zero
            // reply with dummy config
            return SchedulerConfig{.type =
                                       SchedulerConfig::SchedulerType::dFCFS};
        }

        double shorts = (double)num_short;
        double longs = (double)num_long;

        double p = longs / (shorts + longs);

        if (p < 0.1) {
            return SchedulerConfig{.type =
                                       SchedulerConfig::SchedulerType::dFCFS};
        } else {
            return SchedulerConfig{.type =
                                       SchedulerConfig::SchedulerType::cFCFS,
                                   .preemption_interval_us = 500};
        }
    }

    // Suggest a config just based on metrics
    SchedulerConfig
    suggest_from_metrics(SchedulerConfig::SchedulerType curr_type) {
        double var = compute_queued_time_var();

        if (curr_type == SchedulerConfig::SchedulerType::dFCFS) {
            double THRESHOLD = 500 * 500; // if stdev >= 500, then we're
                                          // likely in a dispersive workload

            printf("Determining scheduler. curr=dFCFS, var=%.2f, thresh=%.2f\n",
                   var, THRESHOLD);

            if (var >= THRESHOLD) {
                return SchedulerConfig{
                    .type = SchedulerConfig::SchedulerType::cFCFS,
                    .preemption_interval_us = 500};
            } else {
                return SchedulerConfig{
                    .type = SchedulerConfig::SchedulerType::dFCFS};
            }
        } else if (curr_type == SchedulerConfig::SchedulerType::cFCFS) {
            double THRESHOLD = 50 * 50; // if stdev < 50, then we're
                                        // likely in a dispersive workload

            printf("Determining scheduler. curr=cFCFS, var=%.2f, thresh=%.2f\n",
                   var, THRESHOLD);

            if (var < THRESHOLD) {
                return SchedulerConfig{
                    .type = SchedulerConfig::SchedulerType::cFCFS,
                    .preemption_interval_us = 500};
            } else {
                return SchedulerConfig{
                    .type = SchedulerConfig::SchedulerType::dFCFS};
            }
        } else {
            throw std::runtime_error("unimplemented");
        }
    }

    // Reset metrics
    void clear() {
        num_short = 0;
        num_long = 0;
        metrics.clear();
    }

private:
    int num_short = 0;
    int num_long = 0;
    std::vector<orca::OrcaMetric> metrics;

    // Compute variance in queued time
    double compute_queued_time_var() {
        double mean = 0.0;
        for (const auto &metric : metrics) {
            printf("adding %llu %.2f\n", metric.queued_time_us, (double)metric.queued_time_us);
            mean += (double)metric.queued_time_us;
        }
        mean /= (double)metrics.size();

        double var = 0.0;
        for (const auto &metric : metrics) {
            double v = (double)metric.queued_time_us - mean;
            var += v * v;
        }
        var /= (double)metrics.size();

        printf("mean=%.2f, var=%.2f\n", mean, var);

        return var;
    }
};

class Orca {
public:
    Orca() {
        // set pipe fd's to dummy values
        stdout_pipe_fd[0] = -1;
        stdout_pipe_fd[1] = -1;
        stderr_pipe_fd[0] = -1;
        stderr_pipe_fd[1] = -1;
    }

    ~Orca() {
        printf("Exiting Orca...\n");

        if (curr_sched_pid != 0) {
            terminate_child(curr_sched_pid);
        }
    }

    // Helper which runs a new scheduler and kills the old one (if it exists).
    void set_scheduler(orca::SchedulerConfig config) {
        if (curr_sched_pid != 0) {
            terminate_child(curr_sched_pid);
        }

        curr_sched_pid = run_scheduler(config, stdout_pipe_fd, stderr_pipe_fd);
    }

    // Returns file descriptor which contains stdout of scheduler process
    int get_sched_stdout_fd() { return stdout_pipe_fd[0]; }

    // Returns file descriptor which contains stderr of scheduler process
    int get_sched_stderr_fd() { return stderr_pipe_fd[0]; }

private:
    int stdout_pipe_fd[2];
    int stderr_pipe_fd[2];
    pid_t curr_sched_pid = 0;

    static pid_t delegate_to_child(std::function<void()> work,
                                   int *stdout_pipe_fd, int *stderr_pipe_fd) {
        if (pipe(stdout_pipe_fd) == -1) {
            panic("pipe");
        }

        if (pipe(stderr_pipe_fd) == -1) {
            panic("pipe");
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            panic("fork");
        }

        if (child_pid == 0) {
            // we are the child

            // set our process group id (pgid) to our own pid
            // this allows our parent to kill us
            if (setpgid(0, 0) == -1) {
                panic("setpgid");
            }

            // close read end of pipes
            close(stdout_pipe_fd[0]);
            close(stderr_pipe_fd[0]);

            // redirect stdout to write end of pipe
            // we do this by closing stdout, then calling dup on write end
            // OS allocates lowest available fd (in this case, fd=1)
            close(STDOUT_FILENO);
            int stdoutfd = dup(stdout_pipe_fd[1]);
            if (stdoutfd != STDOUT_FILENO) {
                panic("dup stdout");
            }

            // redirect stderr to write end of pipe
            close(STDERR_FILENO);
            int stderrfd = dup(stderr_pipe_fd[1]);
            if (stderrfd != STDERR_FILENO) {
                panic("dup stderr");
            }

            work();
            exit(0);
        } else {
            // we are the parent

            // close write end of pipes
            close(stdout_pipe_fd[1]);
            close(stderr_pipe_fd[1]);

            return child_pid;
        }
    }

    static void terminate_child(pid_t child_pid) {
        if (kill(child_pid, SIGINT) == -1) {
            panic("kill");
        }

        printf("killing child process (pid=%d) ...\n", child_pid);
        int status;
        waitpid(child_pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("child (pid=%d) exited with status %d\n", child_pid, status);
        } else if (WIFSIGNALED(status)) {
            printf("child (pid=%d) terminated by signal %d\n", child_pid,
                   WTERMSIG(status));
        } else {
            printf("child (pid=%d) ended in unknown way\n", child_pid);
        }
    }

    // Set args to the arguments pointed to by arglist.
    // This function provides a more friendly C++ wrapper for setting dynamic
    // execv arguments.
    template <size_t MaxNumArgs, size_t MaxStrSize>
    static void set_argbuf(char (&argbuf)[MaxNumArgs][MaxStrSize],
                           const std::vector<std::string> &arglist) {
        if (arglist.size() > MaxNumArgs - 1) {
            panic("too many args in arglist");
        }
        for (size_t i = 0; i < arglist.size(); ++i) {
            const auto &s = arglist[i];
            if (s.size() > MaxStrSize - 1) {
                panic("arg length too long");
            }
            strncpy(argbuf[i], s.c_str(), s.size() + 1);
        }
    }

    static bool file_exists(const char *filepath) {
        struct stat buf;
        return stat(filepath, &buf) == 0;
    }

    // Run a scheduling agent.
    // Returns the PID of the scheduler.
    static pid_t run_scheduler(orca::SchedulerConfig config,
                               int *stdout_pipe_fd, int *stderr_pipe_fd) {
        // statically allocate memory for execv args
        // this is kinda sketchy but it should work, since only one scheduling
        // agent runs at a time
        static char argbuf[20][100];

        std::vector<std::string> arglist = {"/usr/bin/sudo"};

        if (config.type == orca::SchedulerConfig::SchedulerType::dFCFS) {
            arglist.push_back("bazel-bin/fifo_per_cpu_agent");
        } else if (config.type == orca::SchedulerConfig::SchedulerType::cFCFS) {
            arglist.push_back("bazel-bin/fifo_centralized_agent");
        } else {
            panic("unrecognized scheduler type");
        }

        arglist.push_back("--ghost_cpus");
        arglist.push_back("0-7");

        // Check if there is an enclave to attach to
        /*
        if (file_exists("/sys/fs/ghost/enclave_1")) {
            if (file_exists("/sys/fs/ghost/enclave_2")) {
                // We expect to make one enclave at most, and to keep reusing it
                // for each scheduler agent
                // If there is more than one enclave, something went wrong
                panic("more enclaves than expected");
            }
            arglist.push_back("--enclave");
            arglist.push_back("/sys/fs/ghost/enclave_1");
        }
        */

        if (config.type != orca::SchedulerConfig::SchedulerType::dFCFS &&
            config.preemption_interval_us >= 0) {
            arglist.push_back("--preemption_time_slice");
            std::ostringstream ss;
            ss << config.preemption_interval_us << "us";
            arglist.push_back(ss.str());
        }

        set_argbuf(argbuf, arglist);

        static char *args[20];
        memset(args, 0, sizeof(args));
        for (size_t i = 0; i < arglist.size(); ++i) {
            args[i] = argbuf[i];
        }

        // print args to scheduler
        for (size_t i = 0; args[i]; ++i) {
            printf("%s ", args[i]);
        }
        printf("\n");

        return delegate_to_child(
            [] {
                execv(args[0], args);
                panic("execv");
            },
            stdout_pipe_fd, stderr_pipe_fd);
    }
};

} // namespace orca
