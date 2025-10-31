#pragma once

namespace Cloudtoid::Interprocess
{
    class IInterprocessSemaphoreReleaser
    {
    public:
        virtual ~IInterprocessSemaphoreReleaser() = default;

        IInterprocessSemaphoreReleaser(const IInterprocessSemaphoreReleaser&) = default;
        IInterprocessSemaphoreReleaser& operator=(const IInterprocessSemaphoreReleaser&) = default;
        IInterprocessSemaphoreReleaser(IInterprocessSemaphoreReleaser&&) = default;
        IInterprocessSemaphoreReleaser& operator=(IInterprocessSemaphoreReleaser&&) = default;

        virtual void Release() = 0;

    protected:
        IInterprocessSemaphoreReleaser() = default;
    };
} // namespace Cloudtoid::Interprocess
