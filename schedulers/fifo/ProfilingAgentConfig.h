#pragma once

#include "lib/enclave.h"
#include "lib/topology.h"

namespace ghost
{
    class ProfilingAgentConfig : public AgentConfig
    {
    public:
        ProfilingAgentConfig() {}

        ProfilingAgentConfig(Topology *topology, CpuList cpulist)
            : AgentConfig(topology, std::move(cpulist)), profiler_cpu(topology->cpu(cpulist.Front().id())) {}

        ProfilingAgentConfig(Topology *topology, CpuList cpulist, Cpu profiler_cpu_)
            : AgentConfig(topology, std::move(cpulist)),
              profiler_cpu(profiler_cpu_) {}

        Cpu profiler_cpu{Cpu::UninitializedType::kUninitialized};
    };

}