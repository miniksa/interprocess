#pragma once
#include <string>

#include "IInterprocessSemaphoreReleaser.h"
#include "IInterprocessSemaphoreWaiter.h"
#include "SemaphoreWindows.h"

namespace Cloudtoid::Interprocess
{
    /// <summary>
    /// This class opens or creates platform agnostic named semaphore. Named
    /// semaphores are synchronization constructs accessible across processes.
    /// </summary>
    class InterprocessSemaphore
    {
    public:
        static IInterprocessSemaphoreReleaser* CreateReleaser(const std::wstring& name)
        {
            return new Semaphore::Windows::SemaphoreWindows(name);
        }

        static IInterprocessSemaphoreWaiter* CreateWaiter(const std::wstring& name)
        {
            return new Semaphore::Windows::SemaphoreWindows(name);
        }
    };
} // namespace Cloudtoid::Interprocess
