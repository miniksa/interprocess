#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <memory>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

int main()
{
    try
    {
        std::cout << "C++ Consumer starting..." << std::endl;

        // Mirror the C# subscriber configuration
        const std::string queueName = "sample-queue";
        const size_t capacity = 1024 * 1024; // 1MB like C# version
        
        // Convert string to wstring for QueueOptions
        const std::wstring wQueueName(queueName.begin(), queueName.end());
        
        // Create queue options - using 2-parameter constructor to match C# behavior exactly
        QueueOptions options(wQueueName, capacity);

        // Create subscriber using the QueueFactory
        QueueFactory factory;
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));

        std::cout << "Connected to queue: " << queueName << std::endl;
        std::cout << "Capacity: " << capacity << " bytes" << std::endl;
        std::cout << "Waiting for messages... (Press Ctrl+C to exit)" << std::endl;
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
                        std::cout << "[" << elapsed << "s] Still waiting for messages... (ensure message sender is running)" << std::endl;
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
        std::cout << "1. No message sender has connected to the queue yet" << std::endl;
        std::cout << "2. There's a permissions issue with the memory-mapped file" << std::endl;
        std::cout << "3. The queue name doesn't match between processes" << std::endl;
        std::cout << std::endl;
        std::cout << "Try running a message sender (like the C# publisher), then this consumer." << std::endl;
        return 1;
    }

    return 0;
}
