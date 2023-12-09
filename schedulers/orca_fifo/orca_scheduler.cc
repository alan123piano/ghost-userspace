#include "schedulers/orca_fifo/orca_scheduler.h"

namespace ghost
{
    void OrcaFifoAgent::AgentThread()
    {
        gtid().assign_name("Agent:" + std::to_string(cpu().id()));
        printf("Agent cpu:=%d, profiler cpu %d\n", cpu().id(), profiler_cpu);
        if (verbose() > 1)
        {
            printf("Agent tid:=%d\n", gtid().tid());
        }
        SignalReady();
        WaitForEnclaveReady();

        PeriodicEdge debug_out(absl::Seconds(1));
        PeriodicEdge profile_peroid(absl::Milliseconds(1));

        while (!Finished() || !per_cpu_scheduler->Empty(cpu()))
        {
            if (*curSched == FIFOSCHEDTYPE::PER_CPU)
                perCpuAgentThread(debug_out, profile_peroid);
            else
                centralizedAgentThread(debug_out, profile_peroid);
        }
    }
    void OrcaFifoAgent::perCpuAgentThread(PeriodicEdge &debug_out, PeriodicEdge &profile_peroid)
    {
        per_cpu_scheduler->Schedule(cpu(), status_word());

        if (profile_peroid.Edge() && cpu().id() == this->profiler_cpu)
        {
            auto res = per_cpu_scheduler->CollectMetric();
            absl::MutexLock lock(&(per_cpu_scheduler->deadTasksMu_));
            for (auto &m : res)
            {
                if (verbose())
                    m.printResult(stderr);
                this->orcaMessenger->sendMessageToOrca(m);
                //   m.sendMessageToOrca();
            }
            for (auto &m : per_cpu_scheduler->deadTasks)
            {
                // printf("-- DEAD-- \n");
                if (verbose())
                    m.printResult(stderr);
                this->orcaMessenger->sendMessageToOrca(m);
                // m.sendMessageToOrca();
            }
            per_cpu_scheduler->deadTasks.clear();
            per_cpu_scheduler->ClearMetric();
            fullOrcaAgent->switchTo();
        }

        if (verbose() && debug_out.Edge())
        {
            static const int flags = verbose() > 1 ? Scheduler::kDumpStateEmptyRQ : 0;
            if (per_cpu_scheduler->debug_runqueue_)
            {
                per_cpu_scheduler->debug_runqueue_ = false;
                per_cpu_scheduler->DumpState(cpu(), Scheduler::kDumpAllTasks);
            }
            else
            {
                per_cpu_scheduler->DumpState(cpu(), flags);
            }
        }
    }

    void OrcaFifoAgent::centralizedAgentThread(PeriodicEdge &debug_out, PeriodicEdge &profile_peroid)
    {
        Channel &centralized_channel = centralized_scheduler->GetDefaultChannel();

        BarrierToken agent_barrier = status_word().barrier();
        // Check if we're assigned as the Global agent.
        if (cpu().id() != centralized_scheduler->GetGlobalCPUId())
        {
            RunRequest *req = enclave()->GetRunRequest(cpu());

            if (verbose() > 1)
            {
                printf("Agent on cpu: %d Idled.\n", cpu().id());
            }
            req->LocalYield(agent_barrier, /*flags=*/0);
        }
        else
        {
            if (boosted_priority() &&
                centralized_scheduler->PickNextGlobalCPU(agent_barrier, cpu()))
            {
                return;
            }

            Message msg;
            while (!(msg = centralized_channel.Peek()).empty())
            {
                centralized_scheduler->DispatchMessage(msg);
                centralized_channel.Consume(msg);
            }

            centralized_scheduler->GlobalSchedule(status_word(), agent_barrier);

            if (profile_peroid.Edge())
            {
                auto res = centralized_scheduler->CollectMetric();
                if (debug_out.Edge())
                {
                    for (auto &m : res)
                    {
                        if (verbose())
                            m.printResult(stderr);
                        this->orcaMessenger->sendMessageToOrca(m);
                    }
                    for (auto &m : centralized_scheduler->deadTasks)
                    {
                        if (verbose())
                            m.printResult(stderr);
                        this->orcaMessenger->sendMessageToOrca(m);
                    }
                    centralized_scheduler->deadTasks.clear();
                    centralized_scheduler->ClearMetric();
                }
                fullOrcaAgent->switchTo();
            }

            if (verbose() && debug_out.Edge())
            {
                static const int flags =
                    verbose() > 1 ? Scheduler::kDumpStateEmptyRQ : 0;
                if (centralized_scheduler->debug_runqueue_)
                {
                    centralized_scheduler->debug_runqueue_ = false;
                    centralized_scheduler->DumpState(cpu(), Scheduler::kDumpAllTasks);
                }
                else
                {
                    centralized_scheduler->DumpState(cpu(), flags);
                }
            }
        }
    }
} // namespace ghost
