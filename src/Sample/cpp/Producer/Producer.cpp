#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <memory>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"

using namespace Cloudtoid::Interprocess;

int main()
{
    try
    {
        std::cout << "C++ Producer starting..." << std::endl;
        
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
        std::cout << "Starting to send messages... (Press Ctrl+C to exit)" << std::endl;
        std::cout << std::endl;
        
        int messageCount = 0;
        auto startTime = std::chrono::steady_clock::now();
        const auto maxDuration = std::chrono::seconds(30); // Run for 30 seconds
        
        // Run for 30 seconds instead of infinite loop
        while (std::chrono::steady_clock::now() - startTime < maxDuration)
        {
            // Create a single byte message: (messageCount % 256)
            // This mirrors exactly what the C# publisher does: (byte)(i % 256)
            unsigned char messageData = static_cast<unsigned char>(messageCount % 256);
            std::span<const unsigned char> message(&messageData, 1);
            
            // Try to enqueue the message
            if (publisher->TryEnqueue(message))
            {
                messageCount++;
                
                // Show progress every 1000 messages
                if (messageCount % 1000 == 0)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
                    
                    double messagesPerSecond = elapsed > 0 ? static_cast<double>(messageCount) / elapsed : 0.0;
                    std::cout << "[" << elapsed << "s] Sent " << messageCount 
                              << " messages (Current byte: " << static_cast<int>(messageData) 
                              << ", Throughput: " << messagesPerSecond << " msg/s)" << std::endl;
                }
            }
            else
            {
                // Queue is full, wait a bit before retrying (like C# version does)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Show debug when queue is full
                if (messageCount % 1000 == 0)
                {
                    std::cout << "DEBUG: Queue full at message " << messageCount 
                              << ", waiting... (byte: " << static_cast<int>(messageData) << ")" << std::endl;
                }
            }
        }
        
        // Print final summary
        auto endTime = std::chrono::steady_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
        double avgThroughput = totalDuration > 0 ? static_cast<double>(messageCount) / totalDuration : 0.0;
        
        std::cout << std::endl;
        std::cout << "=== C++ Producer Finished ===" << std::endl;
        std::cout << "Total runtime: " << totalDuration << " seconds" << std::endl;
        std::cout << "Total messages sent: " << messageCount << std::endl;
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
        std::cout << "Make sure no other publisher is running on the same queue." << std::endl;
        return 1;
    }
    
    return 0;
}
