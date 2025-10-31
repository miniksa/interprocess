#pragma once

#include "IPublisher.h"
#include "ISubscriber.h"
#include "QueueOptions.h"

namespace Cloudtoid::Interprocess
{
    /// <summary>Factory to create queue publishers and subscribers. </summary>
    class IQueueFactory
    {
    public:
        virtual ~IQueueFactory() = default;
        /// <summary> Creates a queue message publisher. </summary>
        virtual IPublisher* CreatePublisher(const QueueOptions& options) = 0;
        /// <summary> Creates a queue message subscriber. </summary>
        virtual ISubscriber* CreateSubscriber(const QueueOptions& options) = 0;
    };
} // namespace Cloudtoid::Interprocess
