#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <cstring>
#include "QueueOptions.h"
#include "Queue.h"
#include "ISubscriber.h"
#include "CircularBuffer.h"
#include "QueueHeader.h"
#include "MessageHeader.h"

using namespace Cloudtoid::Interprocess;

// Complete Subscriber implementation that mirrors the C# version
class Subscriber : public Queue, public ISubscriber
{
public:
    explicit Subscriber(const QueueOptions& options) : Queue(options) {}

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

            // Check if there are any messages available (using public fields)
            if (header->IsEmpty())
            {
                message = std::span<unsigned char>();
                return false;
            }

            // Get the current read position
            auto readPosition = header->ReadOffset;

            // Read the message header - using CircularBuffer::Read method
            std::vector<unsigned char> headerBuffer(sizeof(MessageHeader));
            auto headerData = _buffer->Read(readPosition, sizeof(MessageHeader), std::span<unsigned char>(headerBuffer));
            if (headerData.size() < sizeof(MessageHeader))
            {
                message = std::span<unsigned char>();
                return false;
            }

            // Copy the header data to a MessageHeader struct
            MessageHeader messageHeader(0, 0); // Default constructor
            std::memcpy(&messageHeader, headerData.data(), sizeof(MessageHeader));

            // Calculate message body length
            auto bodyLength = static_cast<size_t>(messageHeader.BodyLength);
            if (bodyLength == 0 || bodyLength > buffer.size())
            {
                // Skip this message if it's empty or too large for the buffer
                auto paddedLength = GetPaddedMessageLength(bodyLength);
                header->ReadOffset = SafeIncrementMessageOffset(readPosition, paddedLength);
                message = std::span<unsigned char>();
                return false;
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

            // Update the read position
            auto paddedLength = GetPaddedMessageLength(bodyLength);
            header->ReadOffset = SafeIncrementMessageOffset(readPosition, paddedLength);

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

            // Sleep briefly before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// Extended QueueFactory that includes CreateSubscriber
class ExtendedQueueFactory
{
public:
    std::unique_ptr<ISubscriber> CreateSubscriber(const QueueOptions& options)
    {
        return std::make_unique<Subscriber>(options);
    }
};

int main()
{
    try
    {
        std::cout << "C++ Consumer starting..." << std::endl;

        // Mirror the C# subscriber configuration
        const std::wstring queueName = L"sample-queue";
        const std::wstring queuePath = L""; // Use default path
        const size_t capacity = 1024 * 1024; // 1MB like C# version

        // Create queue options - using 3-parameter constructor to match C# behavior
        QueueOptions options(queueName, queuePath, capacity);

        // Create subscriber using our extended factory
        ExtendedQueueFactory factory;
        auto subscriber = factory.CreateSubscriber(options);

        std::wcout << L"Connected to queue: " << queueName << std::endl;
        std::cout << "Capacity: " << capacity << " bytes" << std::endl;
        std::cout << "Waiting for messages from C# publisher... (Press Ctrl+C to exit)" << std::endl;
        std::cout << std::endl;

        // Buffer to receive data - single byte like C# version
        std::vector<unsigned char> buffer(1);
        std::span<unsigned char> bufferSpan(buffer);
        std::span<unsigned char> message;

        int messageCount = 0;
        auto startTime = std::chrono::steady_clock::now();

        while (true)
        {
            // Try to dequeue a message
            if (subscriber->TryDequeue(bufferSpan, message))
            {
                if (!message.empty())
                {
                    unsigned char receivedByte = message[0];
                    messageCount++;

                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

                    std::cout << "[" << elapsed << "s] Message " << messageCount
                              << ": Received byte " << static_cast<int>(receivedByte) << std::endl;

                    // Show throughput every 100 messages
                    if (messageCount % 100 == 0)
                    {
                        double messagesPerSecond = elapsed > 0 ? static_cast<double>(messageCount) / elapsed : 0.0;
                        std::cout << "  → Throughput: " << messagesPerSecond << " messages/second" << std::endl;
                    }
                }
            }
            else
            {
                // No message available, sleep briefly to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // Show periodic status
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
                if (elapsed > 0 && elapsed % 30 == 0 && messageCount == 0)
                {
                    static int lastStatusTime = -1;
                    if (elapsed != lastStatusTime)
                    {
                        std::cout << "[" << elapsed << "s] Still waiting for messages... (make sure C# publisher is running)" << std::endl;
                        lastStatusTime = static_cast<int>(elapsed);
                    }
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        std::cout << std::endl;
        std::cout << "This might happen if:" << std::endl;
        std::cout << "1. The C# publisher hasn't created the queue yet" << std::endl;
        std::cout << "2. There's a permissions issue with the memory-mapped file" << std::endl;
        std::cout << "3. The queue name doesn't match between C# and C++" << std::endl;
        std::cout << std::endl;
        std::cout << "Try running the C# publisher first, then this consumer." << std::endl;
        return 1;
    }

    return 0;
}
