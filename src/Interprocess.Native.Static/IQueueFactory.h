#pragma once

#include "IPublisher.h"
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
    };
} // namespace Cloudtoid::Interprocess
