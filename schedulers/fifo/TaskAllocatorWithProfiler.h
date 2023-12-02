#ifndef GHOST_TASK_WITH_PROFILER_H
#define GHOST_TASK_WITH_PROFILER_H

#include "lib/scheduler.h"

#include "schedulers/fifo/TaskWithMetric.h"

namespace ghost
{
    class SingleThreadMallocTaskAllocatorWithProfiler
        : public SingleThreadMallocTaskAllocator<TaskWithMetric>
    {
    public:
        void FreeTask(TaaskWithMetric *task) override
        {
            task->updateState("Died");
            Parent::FreeTask(task);
        }

    private:
        using Parent = SingleThreadMallocTaskAllocator<TaaskWithMetric>;
    };

    class ThreadSafeMallocTaskAllocatorWithProfiler
        : public ThreadSafeMallocTaskAllocator<TaaskWithMetric>
    {
    public:
        void FreeTask(TaaskWithMetric *task) override
        {
            absl::MutexLock lock(&Parent::mu_);
            task->updateState("Died");
            Base::FreeTask(task); // non-guarded free task
        }

    private:
        using Base = SingleThreadMallocTaskAllocator<TaaskWithMetric>;
    };

}

#endif