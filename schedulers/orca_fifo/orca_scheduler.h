#pragma once

#include <memory>

#include "lib/agent.h"
#include "lib/scheduler.h"

#include "schedulers/orca_fifo/per_cpu/fifo_scheduler.h"
#include "schedulers/orca_fifo/centralized/fifo_scheduler.h"

namespace ghost
{

    template <class EnclaveType>
    class FullFifoAgent : public FullAgent<EnclaveType>
    {
    public:
        explicit FullFifoAgent(ProfilingAgentConfig config) : FullAgent<EnclaveType>(config), profiler_cpu(config.profiler_cpu)
        {
            scheduler_ =
                MultiThreadedFifoScheduler(&this->enclave_, *this->enclave_.cpus());

            orcaMessenger = std::make_unique<OrcaMessenger>();
            this->StartAgentTasks();
            this->enclave_.Ready();
        }

        ~FullFifoAgent() override
        {
            this->TerminateAgentTasks();
        }

        std::unique_ptr<Agent> MakeAgent(const Cpu &cpu) override
        {
            return std::make_unique<FifoAgent>(&this->enclave_, cpu, scheduler_.get(), profiler_cpu, orcaMessenger.get());
        }

        void RpcHandler(int64_t req, const AgentRpcArgs &args,
                        AgentRpcResponse &response) override
        {
            switch (req)
            {
            case FifoScheduler::kDebugRunqueue:
                scheduler_->debug_runqueue_ = true;
                response.response_code = 0;
                return;
            case FifoScheduler::kCountAllTasks:
                response.response_code = scheduler_->CountAllTasks();
                return;
            default:
                response.response_code = -1;
                return;
            }
        }

    private:
        std::unique_ptr<FifoScheduler> scheduler_;
        std::unique_ptr<OrcaMessenger> orcaMessenger;
        int32_t profiler_cpu;
    };
}