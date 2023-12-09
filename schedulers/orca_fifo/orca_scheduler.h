#pragma once

#include <memory>

#include "lib/agent.h"
#include "lib/scheduler.h"

#include "schedulers/orca_fifo/per_cpu/fifo_scheduler.h"
#include "schedulers/orca_fifo/centralized/fifo_scheduler.h"

namespace ghost
{
    template <class EnclaveType>
    class FullOrcaFifoAgent;

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

    class OrcaFifoAgent : public LocalAgent
    {
    public:
        OrcaFifoAgent(Enclave *enclave, Cpu cpu, per_cpu::FifoScheduler *per_cpu_scheduler, centralized::FifoScheduler *centralized_scheduler,
                      int32_t _profiler_cpu, OrcaMessenger *_orcaMessenger, FIFOSCHEDTYPE _sched, FullOrcaFifoAgent<LocalEnclave> *fa)
            : LocalAgent(enclave, cpu), per_cpu_scheduler(per_cpu_scheduler), centralized_scheduler(centralized_scheduler),
              profiler_cpu(_profiler_cpu), orcaMessenger(_orcaMessenger), curSched(_sched), fullOrcaAgent(fa)
        {
        }
        void AgentThread() override;
        void perCpuAgentThread();
        void centralizedAgentThread();
        Scheduler *AgentScheduler() const override
        {
            if (curSched == FIFOSCHEDTYPE::CENT)
                return centralized_scheduler;
            else
                return per_cpu_scheduler;
        }

    private:
        per_cpu::FifoScheduler *per_cpu_scheduler;
        centralized::FifoScheduler *centralized_scheduler;
        int32_t profiler_cpu;
        OrcaMessenger *orcaMessenger;
        FIFOSCHEDTYPE curSched;
        FullOrcaFifoAgent<LocalEnclave> *fullOrcaAgent;
    };

    template <class EnclaveType>
    class FullOrcaFifoAgent : public FullAgent<EnclaveType>
    {
    public:
        explicit FullOrcaFifoAgent(OrcaFifoAgentConfig config) : FullAgent<EnclaveType>(config), profiler_cpu(config.profiler_cpu), global_cpu(config.global_cpu_),
                                                                 preemption_time_slice(config.preemption_time_slice_)
        {
            orcaMessenger = std::make_unique<OrcaMessenger>();
            initPerCPU();
            initCent();
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
            centralized_scheduler = centralized::SingleThreadFifoScheduler(&this->enclave_, *this->enclave_.cpus(), this->global_cpu, this->preemption_time_slice);
            this->StartAgentTasks();
            this->enclave_.Ready();
        }
        void switchTo()
        {
            if (currentSched == FIFOSCHEDTYPE::CENT)
            {
                printf("Switch To PER_CPU\n");
                // destroyCent();
                // this->TerminateAgentTasks();
                // centralized_scheduler.reset(nullptr);
                for (auto &agent : this->agents_)
                {
                    dynamic_cast<OrcaFifoAgent *>(agent)->curSched = FIFOSCHEDTYPE::PER_CPU;
                }
                currentSched = FIFOSCHEDTYPE::PER_CPU;
                // initPerCPU();
            }
            else
            {
                printf("Switch To CENTRALIZED\n");
                // this->TerminateAgentTasks();
                // per_cpu_scheduler.reset(nullptr);
                for (auto &agent : this->agents_)
                {
                    dynamic_cast<OrcaFifoAgent *>(agent)->curSched = FIFOSCHEDTYPE::CENT;
                }
                currentSched = FIFOSCHEDTYPE::CENT;
                // initCent();
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

        ~FullOrcaFifoAgent() override
        {
            if (currentSched == FIFOSCHEDTYPE::CENT)
            {
                destroyCent();
            }
            this->TerminateAgentTasks();
        }

        std::unique_ptr<Agent> MakeAgent(const Cpu &cpu) override
        {
            return std::make_unique<OrcaFifoAgent>(&this->enclave_, cpu, per_cpu_scheduler.get(), centralized_scheduler.get(),
                                                   profiler_cpu, orcaMessenger.get(), currentSched, this);
        }

        void RpcHandler(int64_t req, const AgentRpcArgs &args,
                        AgentRpcResponse &response) override
        {
            switch (req)
            {
            case 1: // centralized::FifoScheduler::kDebugRunqueue:
                if (currentSched == FIFOSCHEDTYPE::PER_CPU)
                    per_cpu_scheduler->debug_runqueue_ = true;
                else
                    centralized_scheduler->debug_runqueue_ = true;
                response.response_code = 0;
                return;
            case 2: // per_cpu::FifoScheduler::kCountAllTasks:
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