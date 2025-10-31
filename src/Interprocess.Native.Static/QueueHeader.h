#pragma once

#include <cstddef>

namespace Cloudtoid::Interprocess
{
    class QueueHeader
    {
    public:
        /// <summary>
        /// Where the next message could potentially be read
        /// </summary>
        unsigned long long ReadOffset;

        /// <summary>
        /// Where the next message could potentially be written
        /// </summary>
        unsigned long long WriteOffset;

        /// <summary>
        /// Time (ticks) at which the read lock was taken. It is set to zero if not lock
        /// </summary>
        unsigned long long ReadLockTimeStamp;

        /// <summary>
        /// Not used and might be used in the future
        /// </summary>
        unsigned long long Reserved;

        bool IsEmpty() const noexcept
        {
            return ReadOffset == WriteOffset;
        }
    };

    // Assert exact same layout as the C# version
    static_assert(sizeof(QueueHeader) == 32, "The QueueHeader must be 32-bytes");
    static_assert(offsetof(QueueHeader, ReadOffset) == 0, "ReadOffset must be at offset 0");
    static_assert(offsetof(QueueHeader, WriteOffset) == 8, "WriteOffset must be at offset 8");
    static_assert(offsetof(QueueHeader, ReadLockTimeStamp) == 16, "ReadLockTimeStamp must be at offset 16");
    static_assert(offsetof(QueueHeader, Reserved) == 24, "Reserved must be at offset 24");
} // namespace Cloudtoid::Interprocess
