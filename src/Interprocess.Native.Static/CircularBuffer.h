#pragma once

#include <algorithm>
#include <span>

namespace Cloudtoid::Interprocess
{
    class CircularBuffer
    {
    public:
        CircularBuffer(unsigned char* buffer, const unsigned long long capacity) :
            _capacity{capacity},
            _buffer{buffer}
        {
        }

        [[nodiscard]]
        unsigned long long GetCapacity() const
        {
            return _capacity;
        }

        [[nodiscard]]
        unsigned char* GetPointer(unsigned long long offset) const
        {
            auto adjustedOffset = offset;
            AdjustedOffset(adjustedOffset);
            return _buffer + adjustedOffset;
        }

        [[nodiscard]]
        std::span<const unsigned char> Read(unsigned long long offset, 
                                            unsigned long long length,
                                            const std::span<unsigned char> resultBuffer) const
        {
            if (length == 0)
            {
                return std::span<const unsigned char>{}; // empty
            }

            auto result = resultBuffer;
            length = std::min(length, result.size());

            auto adjustedOffset = offset;
            AdjustedOffset(adjustedOffset);

            const auto resultBufferPtr = result.data();
            const auto sourcePtr = _buffer + adjustedOffset;

            const auto rightLength = std::min(_capacity - adjustedOffset, length);
            if (rightLength > 0)
            {
                std::copy_n(sourcePtr, rightLength, resultBufferPtr);
            }

            const auto leftLength = length - rightLength;
            if (leftLength > 0)
            {
                std::copy_n(_buffer, leftLength, resultBufferPtr + rightLength);
            }

            return result.subspan(0, length);
        }

        void Write(const std::span<const unsigned char> source, const unsigned long long offset) const
        {
            Write(source.data(), source.size(), offset);
        }

        template <typename T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, std::span<const unsigned char>>)
        void Write(const T& source, const unsigned long long offset)
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            Write(reinterpret_cast<const unsigned char*>(&source), sizeof(T), offset);
        }

        void Clear(unsigned long long offset, const unsigned long long length) const
        {
            if (length == 0)
            {
                return;
            }

            auto adjustedOffset = offset;
            AdjustedOffset(adjustedOffset);
            const auto rightLength = std::min(_capacity - adjustedOffset, length);
            std::memset(_buffer + adjustedOffset, 0, rightLength);

            const auto leftLength = length - rightLength;
            if (leftLength > 0)
            {
                std::memset(_buffer, 0, leftLength);
            }
        }

        void AdjustedOffset(unsigned long long& offset) const
        {
            offset %= _capacity;
        }

        void Write(const unsigned char* source, const unsigned long long length, unsigned long long offset) const
        {
            if (length == 0)
            {
                return;
            }

            auto adjustedOffset = offset;
            AdjustedOffset(adjustedOffset);
            const auto rightLength = std::min(_capacity - adjustedOffset, length);
            std::copy_n(source, rightLength, _buffer + adjustedOffset);

            const auto leftLength = length - rightLength;
            if (leftLength > 0)
            {
                std::copy_n(source + rightLength, leftLength, _buffer);
            }
        }

    private:
        unsigned long long _capacity;
        unsigned char* _buffer;
    };
} // namespace Cloudtoid::Interprocess
