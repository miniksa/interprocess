#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <memory>
#include <cstdlib>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

int main(int argc, char* argv[])
{
    try
    {
        // Parse command line arguments for expected message count and optional queue name
        int expectedMessageCount = 100; // Default expectation
        std::string queueName = "sample-queue"; // Default queue name

        if (argc > 1)
        {
            expectedMessageCount = std::atoi(argv[1]);
            if (expectedMessageCount <= 0)
            {
                std::cerr << "Error: Expected message count must be a positive integer" << std::endl;
                std::cerr << "Usage: " << argv[0] << " [expected_message_count] [queue_name]" << std::endl;
                return 1;
            }
        }

        if (argc > 2)
        {
            queueName = argv[2];
        }

        std::cout << "C++ Consumer starting..." << std::endl;
        std::cout << "Expected message count: " << expectedMessageCount << std::endl;

        // Mirror the C# subscriber configuration
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
        std::cout << "Waiting for messages..." << std::endl;
        std::cout << std::endl;

        // Buffer to receive data - single byte like C# version
        std::vector<unsigned char> buffer(1);
        std::span<unsigned char> bufferSpan(buffer);
        std::span<unsigned char> message;

        int messageCount = 0;
        bool sequenceError = false;
        std::vector<int> receivedValues; // Track all received values for final validation
        auto startTime = std::chrono::steady_clock::now();
        auto lastMessageTime = startTime;

        while (true)
        {
            // Try to dequeue a message
            if (subscriber->TryDequeue(bufferSpan, message))
            {
                if (!message.empty())
                {
                    unsigned char receivedByte = message[0];
                    receivedValues.push_back(static_cast<int>(receivedByte));

                    // Validate sequential value (0-99, repeating)
                    int expectedByte = messageCount % 100;
                    if (receivedByte != expectedByte)
                    {
                        if (!sequenceError) // Only log first sequence error
                        {
                            std::cout << "⚠ SEQUENCE ERROR at message " << (messageCount + 1)
                                      << ": expected " << expectedByte << ", got " << static_cast<int>(receivedByte) << std::endl;
                            sequenceError = true;
                        }
                    }

                    messageCount++;
                    lastMessageTime = std::chrono::steady_clock::now();

                    // Show progress every 10 messages or at expected completion
                    if (messageCount % 10 == 0 || messageCount == expectedMessageCount)
                    {
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(lastMessageTime - startTime).count();
                        double throughput = elapsed > 0 ? (messageCount * 1000.0) / elapsed : 0.0;

                        std::cout << "Received " << messageCount << "/" << expectedMessageCount
                                  << " messages (Value: " << static_cast<int>(receivedByte)
                                  << ", Expected: " << expectedByte
                                  << ", Throughput: " << throughput << " msg/s)" << std::endl;
                    }

                    // Break if we've received all expected messages
                    if (messageCount >= expectedMessageCount)
                    {
                        std::cout << std::endl;
                        std::cout << "=== C++ Consumer Finished ===" << std::endl;
                        break;
                    }
                }
            }
            else
            {
                // No message available, check timeout
                auto currentTime = std::chrono::steady_clock::now();
                auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastMessageTime).count();
                auto timeSinceStart = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

                // If no messages for 3 seconds and we've received some, assume producer is done
                // OR if no messages for 10 seconds total (in case producer never starts)
                if ((messageCount > 0 && timeSinceLastMessage >= 3) || timeSinceStart >= 10)
                {
                    std::cout << std::endl;
                    if (messageCount == 0)
                    {
                        std::cout << "=== C++ Consumer Timeout (No messages received) ===" << std::endl;
                    }
                    else
                    {
                        std::cout << "=== C++ Consumer Finished ===" << std::endl;
                    }
                    break;
                }

                // Brief sleep to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Print final summary
        auto endTime = std::chrono::steady_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        double avgThroughput = totalDuration > 0 ? (messageCount * 1000.0) / totalDuration : 0.0;

        std::cout << "Total runtime: " << totalDuration << " ms" << std::endl;
        std::cout << "Total messages received: " << messageCount << std::endl;
        std::cout << "Expected messages: " << expectedMessageCount << std::endl;
        std::cout << "Average throughput: " << avgThroughput << " msg/s" << std::endl;

        // Validate final sequence integrity
        if (messageCount == expectedMessageCount && !sequenceError)
        {
            std::cout << "✓ SUCCESS: Perfect message integrity! All " << messageCount << " messages received in correct sequence (0-" << ((expectedMessageCount - 1) % 100) << ")" << std::endl;
        }
        else if (messageCount != expectedMessageCount)
        {
            std::cout << "✗ ERROR: Message count mismatch. Expected " << expectedMessageCount << ", received " << messageCount << std::endl;
        }
        else if (sequenceError)
        {
            std::cout << "✗ ERROR: Sequence validation failed. Messages received out of order or with incorrect values." << std::endl;
        }

        std::cout << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        std::cout << std::endl;
        std::cout << "This might happen if:" << std::endl;
        std::cout << "1. No message sender has connected to the queue yet" << std::endl;
        std::cout << "2. There's a permissions issue with the memory-mapped file" << std::endl;
        std::cout << "3. The queue configuration doesn't match between processes" << std::endl;
        std::cout << std::endl;
        std::cout << "3. The queue configuration doesn't match between processes" << std::endl;
        std::cout << std::endl;
        std::cout << "Make sure the message producer is running with compatible settings." << std::endl;
        return 1;
    }

    return 0;
}
