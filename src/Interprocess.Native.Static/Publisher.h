#pragma once

#include "IInterprocessSemaphoreReleaser.h"
#include "InterprocessSemaphore.h"
#include "IPublisher.h"
#include "Queue.h"

namespace Cloudtoid::Interprocess
{
    class Publisher final : public Queue, public IPublisher
    {
        IInterprocessSemaphoreReleaser* _signal;

    public:
        explicit Publisher(const QueueOptions& options) :
            Queue(options),
            _signal(InterprocessSemaphore::CreateReleaser(options.GetQueueName()))
        {
        }

        ~Publisher() override
        {
            if (_signal != nullptr)
            {
                delete _signal;
                _signal = nullptr;
            }
        }

        Publisher(const Publisher&) = delete;
        Publisher& operator=(const Publisher&) = delete;
        Publisher(Publisher&&) = default;
        Publisher& operator=(Publisher&&) = default;

        bool TryEnqueue(std::span<const unsigned char> message) override
        {
            auto bodyLength = message.size();
            auto messageLength = GetPaddedMessageLength(bodyLength);

            while (true)
            {
                auto header = *GetHeader();

                if (!CheckCapacity(header, messageLength))
                {
                    return false;
                }

                auto writeOffset = header.WriteOffset;
                auto newWriteOffset = SafeIncrementMessageOffset(writeOffset, messageLength);

                // try to atomically update the write-offset that is stored in the queue header
                if (InterlockedCompareExchange(&header.WriteOffset, newWriteOffset, writeOffset) == writeOffset)
                {
                    // write the message body
                    _buffer->Write(message, GetMessageBodyOffset(writeOffset));

                    // write the message header
                    _buffer->Write(MessageHeader{MessageHeader::ReadyToBeConsumedState, static_cast<int>(bodyLength)},
                                   writeOffset);

                    // signal the next receiver that there is a new message in the queue
                    _signal->Release();
                    return true;
                }
            }
        }

    private:
        bool CheckCapacity(const QueueHeader& header, unsigned long long messageLength)
        {
            if (messageLength > _buffer->GetCapacity())
            {
                return false;
            }

            if (header.IsEmpty())
            {
                return true; // it is an empty queue
            }

            auto readOffset = header.ReadOffset % _buffer->GetCapacity();
            auto writeOffset = header.WriteOffset % _buffer->GetCapacity();

            if (readOffset == writeOffset)
            {
                return false; // queue is full
            }

            if (readOffset < writeOffset)
            {
                if (messageLength > _buffer->GetCapacity() + readOffset - writeOffset)
                {
                    return false;
                }
            }
            else if (messageLength > readOffset - writeOffset)
            {
                return false;
            }

            return true;
        }
    };
} // namespace Cloudtoid::Interprocess
