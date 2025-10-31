#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <memory>
#include <cstdlib>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"

using namespace Cloudtoid::Interprocess;

int main(int argc, char* argv[])
{
    try
    {
        // Parse command line arguments for message count
        int targetMessageCount = 1000; // Default to 1000 messages
        
        if (argc > 1)
        {
            targetMessageCount = std::atoi(argv[1]);
            if (targetMessageCount <= 0)
            {
                std::cerr << "Error: Message count must be a positive integer" << std::endl;
                std::cerr << "Usage: " << argv[0] << " [message_count]" << std::endl;
                return 1;
            }
        }
        
        std::cout << "C++ Producer starting..." << std::endl;
        std::cout << "Target message count: " << targetMessageCount << std::endl;
        
        // Mirror the C# publisher configuration
        const std::string queueName = "sample-queue";
        const size_t capacity = 1024 * 1024; // 1MB like C# version
        
        // Convert string to wstring for QueueOptions
        const std::wstring wQueueName(queueName.begin(), queueName.end());
        
        // Create queue options - using 2-parameter constructor to match C# behavior exactly
        QueueOptions options(wQueueName, capacity);
        
        // Create publisher using the QueueFactory
        QueueFactory factory;
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        
        std::cout << "Created queue: " << queueName << std::endl;
        std::cout << "Capacity: " << capacity << " bytes" << std::endl;
        std::cout << "Starting to send " << targetMessageCount << " messages..." << std::endl;
        std::cout << std::endl;
        
        int messageCount = 0;
        auto startTime = std::chrono::steady_clock::now();
        
        // Send exactly the target number of messages
        while (messageCount < targetMessageCount)
        {
            // Create a single byte message: (messageCount % 256)
            // This mirrors exactly what the C# publisher does: (byte)(i % 256)
            unsigned char messageData = static_cast<unsigned char>(messageCount % 256);
            std::span<const unsigned char> message(&messageData, 1);
            
            // Try to enqueue the message
            if (publisher->TryEnqueue(message))
            {
                messageCount++;
                
                // Show progress every 100 messages or at key milestones
                if (messageCount % 100 == 0 || messageCount == targetMessageCount)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
                    
                    double messagesPerSecond = elapsed > 0 ? static_cast<double>(messageCount * 1000) / elapsed : 0.0;
                    std::cout << "Sent " << messageCount << "/" << targetMessageCount
                              << " messages (Current byte: " << static_cast<int>(messageData) 
                              << ", Throughput: " << messagesPerSecond << " msg/s)" << std::endl;
                }
            }
            else
            {
                // Queue is full, wait a bit before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Print final summary
        auto endTime = std::chrono::steady_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        double avgThroughput = totalDuration > 0 ? static_cast<double>(messageCount * 1000) / totalDuration : 0.0;
        
        std::cout << std::endl;
        std::cout << "=== C++ Producer Finished ===" << std::endl;
        std::cout << "Total runtime: " << totalDuration << " ms" << std::endl;
        std::cout << "Total messages sent: " << messageCount << std::endl;
        std::cout << "Target messages: " << targetMessageCount << std::endl;
        std::cout << "Average throughput: " << avgThroughput << " msg/s" << std::endl;
        std::cout << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        std::cout << std::endl;
        std::cout << "This might happen if:" << std::endl;
        std::cout << "1. Unable to create the memory-mapped file" << std::endl;
        std::cout << "2. Permissions issue with the queue creation" << std::endl;
        std::cout << "3. Another process has the queue locked" << std::endl;
        std::cout << std::endl;
        std::cout << "Check that no other process is using the same queue name with conflicting settings." << std::endl;
        return 1;
    }
    
    return 0;
}
