#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "QueueHeader.h"

namespace Cloudtoid::Interprocess
{
    /// <summary> The options to create a queue. </summary>
    class QueueOptions
    {
    public:
        /// <summary>
        /// Initializes a new instance of the <see cref="QueueOptions"/> class.
        /// </summary>
        /// <param name="queueName">The unique name of the queue.</param>
        /// <param name="capacity">The maximum capacity of the queue in bytes. This should be at least 16 bytes long and in the multiples of 8</param>
        QueueOptions(const std::wstring_view queueName, const unsigned long long capacity) :
            QueueOptions(queueName, {}, capacity)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="QueueOptions"/> class.
        /// </summary>
        /// <param name="queueName">The unique name of the queue.</param>
        /// <param name="path">The path to the directory/folder in which the memory mapped and other files are stored in</param>
        /// <param name="capacity">The maximum capacity of the queue in bytes. This should be at least 16 bytes long and in the multiples of 8</param>
        QueueOptions(const std::wstring_view queueName, const std::wstring_view path, const unsigned long long capacity)
        {
            if (queueName.empty())
            {
                throw std::invalid_argument("queueName");
            }
            _queueName = queueName;

            _path = path;

            if (capacity < 16)
            {
                throw std::invalid_argument("capacity");
            }

            if (capacity % 8 != 0)
            {
                throw std::invalid_argument("capacity must be a multiple of 8");
            }
            _capacity = capacity;
        }

        /// <summary>
        /// Gets the unique name of the queue.
        /// </summary>
        [[nodiscard]]
        const std::wstring& GetQueueName() const noexcept
        {
            return _queueName;
        }

        /// <summary>
        /// Gets the path to the directory/folder in which the memory mapped and other files are stored in.
        /// </summary>
        [[nodiscard]]
        std::wstring_view GetPath() const noexcept
        {
            return _path;
        }

        /// <summary>
        /// Gets the size of the queue in bytes. This does NOT include the space needed for the queue header.
        /// </summary>
        [[nodiscard]]
        unsigned long long GetCapacity() const noexcept
        {
            return _capacity;
        }

        /// <summary>
        /// Gets the full size of the queue that includes both the header and message sections
        /// </summary>
        unsigned long long GetQueueStorageSize() const noexcept
        {
            return sizeof(QueueHeader) + _capacity;
        }

    private:
        std::wstring _queueName;
        std::wstring _path;
        unsigned long long _capacity;
    };
} // namespace Cloudtoid::Interprocess
