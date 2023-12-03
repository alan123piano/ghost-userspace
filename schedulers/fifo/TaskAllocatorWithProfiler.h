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
        void FreeTask(TaskWithMetric *task) override
        {
            task->updateState("Died");
            Parent::FreeTask(task);
        }

    private:
        using Parent = SingleThreadMallocTaskAllocator<TaskWithMetric>;
    };

    class ThreadSafeMallocTaskAllocatorWithProfiler
        : public ThreadSafeMallocTaskAllocator<TaskWithMetric>
    {
    public:
        void FreeTask(TaskWithMetric *task) override
        {
            absl::MutexLock lock(&mu_);
            task->updateState("Died");
            Base::FreeTask(task); // non-guarded free task
        }

    private:
        using Base = SingleThreadMallocTaskAllocator<TaskWithMetric>;
    };

}

#endif