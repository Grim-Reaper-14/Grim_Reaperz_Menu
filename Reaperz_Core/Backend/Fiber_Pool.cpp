#include "FiberPool.h"
#include <Windows.h>
#include <cassert>

namespace Grim_Reaperz_Menu
{
    // Fiber entry point wrapper to call ScriptEntry
    void CALLBACK FiberEntry(LPVOID lpParameter)
    {
        FiberPool* pool = static_cast<FiberPool*>(lpParameter);
        pool->ScriptEntry();
    }

    void FiberPool::InitImpl(int num_fibers)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);

        // Ensure the fiber pool isn't already initialized
        assert(m_Jobs.empty() && "FiberPool already initialized");

        // Convert the current thread to a fiber if not already
        if (!GetCurrentFiber())
        {
            ConvertThreadToFiber(nullptr);
        }

        // Create the specified number of fibers
        for (int i = 0; i < num_fibers; ++i)
        {
            void* fiber = CreateFiber(0, FiberEntry, this);
            if (fiber)
            {
                m_Jobs.push([fiber]() {
                    SwitchToFiber(fiber);
                });
            }
        }
    }

    void FiberPool::DestroyImpl()
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);

        // Clear the job stack
        while (!m_Jobs.empty())
        {
            m_Jobs.pop();
        }

        // Delete all fibers
        for (auto& fiber : m_Jobs)
        {
            DeleteFiber(fiber.target<void(*)()>());
        }

        // Convert the main thread back to a normal thread if needed
        ConvertFiberToThread();
    }

    void FiberPool::PushImpl(std::function<void()> callback)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        m_Jobs.push(std::move(callback));
    }

    void FiberPool::Tick()
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);

        if (!m_Jobs.empty())
        {
            // Pop the next task
            auto job = std::move(m_Jobs.top());
            m_Jobs.pop();

            // Execute the task
            job();
        }
    }

    void FiberPool::ScriptEntry()
    {
        // Fiber loop: keep running tasks until the pool is destroyed
        while (true)
        {
            std::function<void()> job;
            {
                std::lock_guard<std::recursive_mutex> lock(m_Mutex);
                if (m_Jobs.empty())
                {
                    // No tasks, yield back to the main thread
                    SwitchToFiber(GetCurrentFiber());
                    continue;
                }

                job = std::move(m_Jobs.top());
                m_Jobs.pop();
            }

            // Execute the task
            job();

            // Yield back to the main thread after each task
            SwitchToFiber(GetCurrentFiber());
        }
    }
}
