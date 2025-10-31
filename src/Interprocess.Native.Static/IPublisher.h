#pragma once

#include <span>

namespace Cloudtoid::Interprocess
{
    /// <summary>
    /// Message publisher that publishes messages to the subscribers.
    /// </summary>
    class IPublisher
    {
    protected:
        IPublisher() = default;
    public:
        virtual ~IPublisher() = default;
        IPublisher(const IPublisher&) = default;
        IPublisher& operator=(const IPublisher&) = default;
        IPublisher(IPublisher&&) = default;
        IPublisher& operator=(IPublisher&&) = default;

        /// <summary>Enqueues the message to be published to the subscribers.</summary>
        virtual bool TryEnqueue(std::span<const unsigned char> message) = 0;
    };
} // namespace Cloudtoid::Interprocess
