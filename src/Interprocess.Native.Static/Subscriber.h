#pragma once

#include <thread>
#include <chrono>
#include <cstring>
#include "IInterprocessSemaphoreWaiter.h"
#include "InterprocessSemaphore.h"
#include "ISubscriber.h"
#include "Queue.h"
#include "MessageHeader.h"

namespace Cloudtoid::Interprocess
{
    class Subscriber final : public Queue, public ISubscriber
    {
        IInterprocessSemaphoreWaiter* _waiter;

    public:
        explicit Subscriber(const QueueOptions& options) :
            Queue(options),
            _waiter(InterprocessSemaphore::CreateWaiter(options.GetQueueName()))
        {
        }

        ~Subscriber() override
        {
            if (_waiter != nullptr)
            {
                delete _waiter;
                _waiter = nullptr;
            }
        }

        Subscriber(const Subscriber&) = delete;
        Subscriber& operator=(const Subscriber&) = delete;
        Subscriber(Subscriber&&) = default;
        Subscriber& operator=(Subscriber&&) = default;

        bool TryDequeue(std::span<unsigned char> buffer, std::span<unsigned char>& message) override
        {
            try
            {
                auto* header = GetHeader();
                if (header == nullptr)
                {
                    message = std::span<unsigned char>();
                    return false;
                }

                // Check if there are any messages available
                if (header->IsEmpty())
                {
                    message = std::span<unsigned char>();
                    return false;
                }

                // Get the current read position
                auto readPosition = header->ReadOffset;

                // Read the message header
                std::vector<unsigned char> headerBuffer(sizeof(MessageHeader));
                auto headerData = _buffer->Read(readPosition, sizeof(MessageHeader), std::span<unsigned char>(headerBuffer));
                if (headerData.size() < sizeof(MessageHeader))
                {
                    message = std::span<unsigned char>();
                    return false;
                }

                // Copy the header data to a MessageHeader struct
                MessageHeader messageHeader(0, 0);
                std::memcpy(&messageHeader, headerData.data(), sizeof(MessageHeader));

                // Validate the message header state
                if (messageHeader.State != MessageHeader::ReadyToBeConsumedState)
                {
                    message = std::span<unsigned char>();
                    return false;
                }

                // Calculate message body length
                auto bodyLength = static_cast<size_t>(messageHeader.BodyLength);
                if (bodyLength == 0 || bodyLength > buffer.size())
                {
                    // Skip this message if it's empty or too large for the buffer
                    auto paddedLength = GetPaddedMessageLength(bodyLength);
                    
                    // Atomically update the read position
                    auto newReadOffset = SafeIncrementMessageOffset(readPosition, paddedLength);
                    InterlockedExchange(&header->ReadOffset, newReadOffset);
                    
                    message = std::span<unsigned char>();
                    return bodyLength > buffer.size(); // Return true if message was too large
                }

                // Read the message body
                auto bodyOffset = GetMessageBodyOffset(readPosition);
                std::vector<unsigned char> bodyBuffer(bodyLength);
                auto bodyData = _buffer->Read(bodyOffset, bodyLength, std::span<unsigned char>(bodyBuffer));
                if (bodyData.size() < bodyLength)
                {
                    message = std::span<unsigned char>();
                    return false;
                }

                // Copy the body data to the buffer
                std::memcpy(buffer.data(), bodyData.data(), bodyLength);

                // Atomically update the read position
                auto paddedLength = GetPaddedMessageLength(bodyLength);
                auto newReadOffset = SafeIncrementMessageOffset(readPosition, paddedLength);
                InterlockedExchange(&header->ReadOffset, newReadOffset);

                // Return the message span
                message = std::span<unsigned char>(buffer.data(), bodyLength);
                return true;
            }
            catch (...)
            {
                message = std::span<unsigned char>();
                return false;
            }
        }

        std::span<unsigned char> Dequeue(std::span<unsigned char> buffer) override
        {
            std::span<unsigned char> message;

            // Keep trying until we get a message
            while (true)
            {
                if (TryDequeue(buffer, message))
                {
                    return message;
                }

                // Wait for a signal that a new message is available
                // Use a timeout to avoid infinite blocking
                constexpr int timeoutMs = 100;
                _waiter->Wait(timeoutMs);
            }
        }
    };
} // namespace Cloudtoid::Interprocess