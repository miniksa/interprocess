#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <span>
#include <memory>
#include <cstdlib>

#define NOMINMAX  // Prevent windows.h from defining min/max macros
#include <windows.h>

#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"

using namespace Cloudtoid::Interprocess;

// Custom producer that sends a specific range of values
// Usage: RangeProducer <start_value> <count> <queue_name>
// Example: RangeProducer 0 5 test-queue  (sends values 0, 1, 2, 3, 4)

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 4)
        {
            std::cerr << "Usage: " << argv[0] << " <start_value> <count> <queue_name> [event_name]" << std::endl;
            std::cerr << "Example: " << argv[0] << " 0 5 test-queue StartEvent" << std::endl;
            return 1;
        }
        
        int startValue = std::atoi(argv[1]);
        int count = std::atoi(argv[2]);
        std::string queueName = argv[3];
        std::string eventName = (argc > 4) ? argv[4] : "";
        
        if (count <= 0)
        {
            std::cerr << "Error: Count must be a positive integer" << std::endl;
            return 1;
        }
        
        std::cout << "Range Producer starting..." << std::endl;
        std::cout << "Start value: " << startValue << std::endl;
        std::cout << "Count: " << count << std::endl;
        std::cout << "Queue: " << queueName << std::endl;
        
        // If event name is specified, wait for it to be signaled before proceeding
        if (!eventName.empty())
        {
            std::cout << "Waiting for start event: " << eventName << std::endl;
            
            // Open or create the named event
            std::wstring wEventName(eventName.begin(), eventName.end());
            HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, wEventName.c_str());
            
            if (hEvent == NULL)
            {
                // Event doesn't exist yet, create it (unsignaled)
                hEvent = CreateEventW(NULL, TRUE, FALSE, wEventName.c_str());
                if (hEvent == NULL)
                {
                    std::cerr << "Failed to create/open event: " << GetLastError() << std::endl;
                    return 1;
                }
            }
            
            std::cout << "Blocking on event..." << std::endl;
            
            // Wait for the event to be signaled (max 30 seconds)
            DWORD waitResult = WaitForSingleObject(hEvent, 30000);
            
            if (waitResult == WAIT_OBJECT_0)
            {
                std::cout << "Event signaled! Starting production..." << std::endl;
            }
            else if (waitResult == WAIT_TIMEOUT)
            {
                std::cerr << "Timeout waiting for start event" << std::endl;
                CloseHandle(hEvent);
                return 1;
            }
            else
            {
                std::cerr << "Error waiting for event: " << GetLastError() << std::endl;
                CloseHandle(hEvent);
                return 1;
            }
            
            CloseHandle(hEvent);
        }
        
        const size_t capacity = 10 * 1024 * 1024; // 10MB for high concurrency
        const std::wstring wQueueName(queueName.begin(), queueName.end());
        
        QueueOptions options(wQueueName, capacity);
        QueueFactory factory;
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        
        std::cout << "Connected to queue, sending values " << startValue 
                  << " to " << (startValue + count - 1) << "..." << std::endl;
        
        int sent = 0;
        auto startTime = std::chrono::steady_clock::now();
        
        for (int i = 0; i < count; ++i)
        {
            int value = startValue + i;
            std::span<const unsigned char> message(reinterpret_cast<const unsigned char*>(&value), sizeof(int));
            
            while (!publisher->TryEnqueue(message))
            {
                // Queue full, wait and retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            sent++;
            std::cout << "Sent value: " << value << " (" << sent << "/" << count << ")" << std::endl;
            
            // Small delay to allow interleaving with other producers
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        std::cout << std::endl;
        std::cout << "Range Producer finished!" << std::endl;
        std::cout << "Sent " << sent << " messages in " << duration << " ms" << std::endl;
        std::cout << "Values sent: " << startValue << " to " << (startValue + count - 1) << std::endl;
        
        // Calculate and display sum for verification
        int sum = 0;
        for (int i = 0; i < count; ++i)
        {
            sum += (startValue + i);
        }
        std::cout << "Sum of sent values: " << sum << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    
    return 0;
}
