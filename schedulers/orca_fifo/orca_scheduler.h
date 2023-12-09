#pragma once

#include <memory>

#include "lib/agent.h"
#include "lib/scheduler.h"

#include "schedulers/orca_fifo/per_cpu/fifo_scheduler.h"
#include "schedulers/orca_fifo/centralized/fifo_scheduler.h"

namespace ghost
{
    enum class FIFOSCHEDTYPE
    {
        PER_CPU,
        CENT
    };

    class OrcaFifoAgentConfig : public ProfilingAgentConfig
    {
    public:
        OrcaFifoAgentConfig() {}

        OrcaFifoAgentConfig(Topology *topology, CpuList cpulist, int32_t profiler_cpu_, Cpu global_cpu,
                            absl::Duration preemption_time_slice)
            : ProfilingAgentConfig(topology, std::move(cpulist), profiler_cpu_),
              global_cpu_(global_cpu.id()),
              preemption_time_slice_(preemption_time_slice) {}

        OrcaFifoAgentConfig(Topology *topology, CpuList cpulist, Cpu global_cpu,
                            absl::Duration preemption_time_slice)
            : ProfilingAgentConfig(topology, std::move(cpulist)),
              global_cpu_(global_cpu.id()),
              preemption_time_slice_(preemption_time_slice) {}

        int32_t global_cpu_;
        absl::Duration preemption_time_slice_ = absl::InfiniteDuration();
    };

    template <class EnclaveType>
    class FullFifoAgent : public FullAgent<EnclaveType>
    {
    public:
        explicit FullFifoAgent(OrcaFifoAgentConfig config) : FullAgent<EnclaveType>(config), profiler_cpu(config.profiler_cpu), global_cpu(config.global_cpu_),
                                                             preemption_time_slice(config.preemption_time_slice_)
        {
            orcaMessenger = std::make_unique<OrcaMessenger>();
            initPerCPU();
        }
        void initPerCPU()
        {
            currentSched = FIFOSCHEDTYPE::PER_CPU;
            per_cpu_scheduler = per_cpu::MultiThreadedFifoScheduler(&this->enclave_, *this->enclave_.cpus());
            this->StartAgentTasks();
            this->enclave_.Ready();
        }
        void initCent()
        {
            currentSched = FIFOSCHEDTYPE::CENT;
            centralized_scheduler = centralized::SingleThreadFifoScheduler(&this->enclave_, *this->enclave_.cpus(), this->global_cpu, this->preemption_time_slice_);
            this->StartAgentTasks();
            this->enclave_.Ready();
        }
        void switchTo(FIFOSCHEDTYPE to)
        {
            if (to == FIFOSCHEDTYPE::PER_CPU)
            {
                CHECK_EQ(currentSched, FIFOSCHEDTYPE::CENT);
                destroyCent();
                this->TerminateAgentTasks();
                delete centralized_scheduler;
                centralized_scheduler = nullptr;
                currentSched = FIFOSCHEDTYPE::PER_CPU;
                initPerCPU();
            }
            else
            {
                CHECK_EQ(currentSched, FIFOSCHEDTYPE::PER_CPU);
                this->TerminateAgentTasks();
                delete per_cpu_scheduler;
                per_cpu_scheduler = nullptr;
                currentSched = FIFOSCHEDTYPE::CENT;
                initCent();
            }
        }

        void destroyCent()
        {
            auto global_cpuid = centralized_scheduler->GetGlobalCPUId();
            if (this->agents_.front()->cpu().id() != global_cpuid)
            {
                // Bring the current globalcpu agent to the front.
                for (auto it = this->agents_.begin(); it != this->agents_.end(); it++)
                {
                    if (((*it)->cpu().id() == global_cpuid))
                    {
                        auto d = std::distance(this->agents_.begin(), it);
                        std::iter_swap(this->agents_.begin(), this->agents_.begin() + d);
                        break;
                    }
                }
            }
            CHECK_EQ(this->agents_.front()->cpu().id(), global_cpuid);
        }

        ~FullFifoAgent() override
        {
            if (currentSched == FIFOSCHEDTYPE::CENT)
            {
                destroyCent();
            }
            this->TerminateAgentTasks();
        }

        std::unique_ptr<Agent> MakeAgent(const Cpu &cpu) override
        {
            if (currentSched == FIFOSCHEDTYPE::PER_CPU)
                return std::make_unique<per_cpu::FifoAgent>(&this->enclave_, cpu, per_cpu_scheduler.get(), profiler_cpu, orcaMessenger.get());
            else
                return std::make_unique<centralized::FifoAgent>(&this->enclave_, cpu, centralized_scheduler.get(), orcaMessenger.get());
        }

        void RpcHandler(int64_t req, const AgentRpcArgs &args,
                        AgentRpcResponse &response) override
        {
            switch (req)
            {
            case per_cpu::FifoScheduler::kDebugRunqueue:
                if (currentSched == FIFOSCHEDTYPE::PER_CPU)
                    per_cpu_scheduler->debug_runqueue_ = true;
                else
                    centralized_scheduler->debug_runqueue_ = true;
                response.response_code = 0;
                return;
            case centralized::FifoScheduler::kDebugRunqueue:
                if (currentSched == FIFOSCHEDTYPE::PER_CPU)
                    per_cpu_scheduler->debug_runqueue_ = true;
                else
                    centralized_scheduler->debug_runqueue_ = true;
                response.response_code = 0;
                return;

            case per_cpu::FifoScheduler::kCountAllTasks:
                if (currentSched == FIFOSCHEDTYPE::PER_CPU)
                    response.response_code = per_cpu_scheduler->CountAllTasks();
                else
                    response.response_code = 0;
                return;
            default:
                response.response_code = -1;
                return;
            }
        }

    private:
        FIFOSCHEDTYPE currentSched;
        std::unique_ptr<per_cpu::FifoScheduler> per_cpu_scheduler;
        std::unique_ptr<centralized::FifoScheduler> centralized_scheduler;
        std::unique_ptr<OrcaMessenger> orcaMessenger;
        int32_t profiler_cpu;
        int32_t global_cpu;
        absl::Duration preemption_time_slice = absl::InfiniteDuration();
    };
}