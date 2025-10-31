#pragma once

#include "IQueueFactory.h"
#include "Publisher.h"

namespace Cloudtoid::Interprocess
{
    // <inheritdoc/>
    class QueueFactory final : public IQueueFactory
    {
    public:
        // <inheritdoc/>
        QueueFactory() = default;
        ~QueueFactory() override = default;
        QueueFactory(const QueueFactory&) = default;
        QueueFactory& operator=(const QueueFactory&) = default;
        QueueFactory(QueueFactory&&) = default;
        QueueFactory& operator=(QueueFactory&&) = default;

        // <inheritdoc/>
        IPublisher* CreatePublisher(const QueueOptions& options) override
        {
            return new Publisher(options);
        }
    };
    static_assert(sizeof(void*) == 8, "64-bit architecture required");
} // namespace Cloudtoid::Interprocess
