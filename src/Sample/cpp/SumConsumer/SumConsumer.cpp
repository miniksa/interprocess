#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <set>
#include <span>
#include <memory>
#include <cstdlib>

#define NOMINMAX  // Prevent windows.h from defining min/max macros
#include <windows.h>

#include "QueueOptions.h"
#include "QueueFactory.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

// Custom consumer that validates sum of received values
// Usage: SumConsumer <expected_count> <expected_sum> <queue_name> [timeout_seconds]
// Example: SumConsumer 15 105 test-queue 30

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 4)
        {
            std::cerr << "Usage: " << argv[0] << " <expected_count> <expected_sum> <queue_name> [timeout_seconds] [event_name]" << std::endl;
            std::cerr << "Example: " << argv[0] << " 15 105 test-queue 30 StartEvent" << std::endl;
            return 1;
        }
        
        int expectedCount = std::atoi(argv[1]);
        int expectedSum = std::atoi(argv[2]);
        std::string queueName = argv[3];
        int timeoutSeconds = (argc > 4) ? std::atoi(argv[4]) : 30;
        std::string eventName = (argc > 5) ? argv[5] : "";
        
        if (expectedCount <= 0)
        {
            std::cerr << "Error: Expected count must be a positive integer" << std::endl;
            return 1;
        }
        
        std::cout << "Sum Consumer starting..." << std::endl;
        std::cout << "Expected message count: " << expectedCount << std::endl;
        std::cout << "Expected sum: " << expectedSum << std::endl;
        std::cout << "Queue: " << queueName << std::endl;
        std::cout << "Timeout: " << timeoutSeconds << " seconds" << std::endl;
        std::cout << std::endl;
        
        const size_t capacity = 10 * 1024 * 1024; // 10MB for high concurrency
        const std::wstring wQueueName(queueName.begin(), queueName.end());
        
        QueueOptions options(wQueueName, capacity);
        QueueFactory factory;
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        std::cout << "Connected to queue" << std::endl;
        
        // If event name specified, signal it to start all producers
        HANDLE hEvent = NULL;
        if (!eventName.empty())
        {
            std::cout << "Signaling start event: " << eventName << std::endl;
            
            std::wstring wEventName(eventName.begin(), eventName.end());
            
            // Create the event in signaled state (manual-reset so all waiters are released)
            hEvent = CreateEventW(NULL, TRUE, FALSE, wEventName.c_str());
            if (hEvent == NULL)
            {
                std::cerr << "Failed to create event: " << GetLastError() << std::endl;
                return 1;
            }
            
            // Signal the event to release all waiting producers
            if (!SetEvent(hEvent))
            {
                std::cerr << "Failed to signal event: " << GetLastError() << std::endl;
                CloseHandle(hEvent);
                return 1;
            }
            
            std::cout << "Event signaled - all producers starting now!" << std::endl;
        }
        
        std::cout << "Waiting for messages..." << std::endl;
        std::cout << std::endl;
        
        std::vector<unsigned char> buffer(sizeof(int));
        std::span<unsigned char> bufferSpan(buffer);
        std::span<unsigned char> message;
        
        int messageCount = 0;
        int actualSum = 0;
        std::vector<int> receivedValues;
        std::set<int> uniqueValues;
        
        auto startTime = std::chrono::steady_clock::now();
        auto lastMessageTime = startTime;
        auto timeoutDuration = std::chrono::seconds(timeoutSeconds);
        
        while (messageCount < expectedCount)
        {
            if (subscriber->TryDequeue(bufferSpan, message))
            {
                if (message.size() >= sizeof(int))
                {
                    int intValue = *reinterpret_cast<const int*>(message.data());
                    
                    receivedValues.push_back(intValue);
                    uniqueValues.insert(intValue);
                    actualSum += intValue;
                    messageCount++;
                    lastMessageTime = std::chrono::steady_clock::now();
                    
                    std::cout << "Received value: " << intValue 
                              << " (message " << messageCount << "/" << expectedCount 
                              << ", running sum: " << actualSum << ")" << std::endl;
                }
            }
            else
            {
                // Check for timeout
                auto now = std::chrono::steady_clock::now();
                auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(now - lastMessageTime);
                
                if (timeSinceLastMessage > timeoutDuration)
                {
                    std::cout << std::endl;
                    std::cout << "⚠ TIMEOUT: No messages received for " << timeoutSeconds << " seconds" << std::endl;
                    break;
                }
                
                // Small sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        std::cout << std::endl;
        std::cout << "=== Sum Consumer Results ===" << std::endl;
        std::cout << "Total runtime: " << duration << " ms" << std::endl;
        std::cout << "Messages received: " << messageCount << " / " << expectedCount << std::endl;
        std::cout << "Actual sum: " << actualSum << std::endl;
        std::cout << "Expected sum: " << expectedSum << std::endl;
        std::cout << "Unique values received: " << uniqueValues.size() << std::endl;
        
        // Display all received values
        std::cout << "Received values: [";
        for (size_t i = 0; i < receivedValues.size(); ++i)
        {
            std::cout << receivedValues[i];
            if (i < receivedValues.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        std::cout << std::endl;
        
        // Validation
        bool success = true;
        std::string errorMsg;
        
        if (messageCount != expectedCount)
        {
            success = false;
            errorMsg = "Message count mismatch";
            std::cout << "❌ FAILED: Expected " << expectedCount << " messages but received " << messageCount << std::endl;
        }
        else if (actualSum != expectedSum)
        {
            success = false;
            errorMsg = "Sum mismatch";
            std::cout << "❌ FAILED: Expected sum " << expectedSum << " but got " << actualSum << std::endl;
            std::cout << "   Difference: " << (actualSum - expectedSum) << std::endl;
        }
        else
        {
            std::cout << "✅ SUCCESS: All messages received with correct sum!" << std::endl;
            std::cout << "   Perfect integrity - no data corruption or loss detected." << std::endl;
        }
        
        std::cout << std::endl;
        
        // Check for duplicates (not necessarily an error, but interesting to know)
        if (uniqueValues.size() != static_cast<size_t>(messageCount))
        {
            std::cout << "ℹ Note: " << (messageCount - uniqueValues.size()) << " duplicate value(s) detected" << std::endl;
            std::cout << "   This is expected when value ranges overlap." << std::endl;
        }
        
        // Cleanup event handle
        if (hEvent != NULL)
        {
            CloseHandle(hEvent);
        }
        
        return success ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
