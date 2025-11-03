#pragma once

namespace Cloudtoid::Interprocess
{
    class IInterprocessSemaphoreWaiter
    {
    public:
        virtual ~IInterprocessSemaphoreWaiter() = default;

        IInterprocessSemaphoreWaiter(const IInterprocessSemaphoreWaiter&) = default;
        IInterprocessSemaphoreWaiter& operator=(const IInterprocessSemaphoreWaiter&) = default;
        IInterprocessSemaphoreWaiter(IInterprocessSemaphoreWaiter&&) = default;
        IInterprocessSemaphoreWaiter& operator=(IInterprocessSemaphoreWaiter&&) = default;

        virtual bool Wait(int millisecondTimeout) = 0;

    protected:
        IInterprocessSemaphoreWaiter() = default;
    };
} // namespace Cloudtoid::Interprocess
