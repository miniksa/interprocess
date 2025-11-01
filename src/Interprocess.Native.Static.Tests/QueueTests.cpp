#include "pch.h"
#include <chrono>
#include <memory>
#include <span>
#include <vector>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

// Basic queue tests - focusing on what can actually be tested
TEST(QueueBasicTests, QueueHeaderLayout) {
    // These are compile-time assertions, but we can also test them at runtime
    EXPECT_EQ(sizeof(unsigned long long), 8); // Ensure we're on a 64-bit system
    EXPECT_GE(sizeof(void*), 8); // 64-bit pointers
}

TEST(QueueBasicTests, BasicMath) {
    // Test some basic functionality that doesn't require the full library
    unsigned long long capacity = 1024;
    EXPECT_EQ(capacity % 8, 0); // Capacity should be multiple of 8
    
    unsigned long long messageLength = 16;
    unsigned long long paddedLength = 8 * static_cast<unsigned long long>(std::ceil(static_cast<double>(messageLength) / 8.0));
    EXPECT_EQ(paddedLength, 16); // 16 is already multiple of 8
    
    messageLength = 17;
    paddedLength = 8 * static_cast<unsigned long long>(std::ceil(static_cast<double>(messageLength) / 8.0));
    EXPECT_EQ(paddedLength, 24); // 17 should round up to 24
}

// Test that a newly created queue has properly initialized header
TEST(QueueInitializationTests, NewQueueHeaderShouldBeInitialized) {
    // Create a unique queue name to ensure we get a fresh queue
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::wstring queueName = L"test-init-queue-" + std::to_wstring(timestamp);
    
    // Create queue options
    QueueOptions options(queueName, 1024 * 1024);
    
    {
        // Create a publisher to initialize the queue
        QueueFactory factory;
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        
        // The queue should now exist and be properly initialized
        // Create a subscriber to check the header state
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        // Try to read from the queue - it should be empty, not contain garbage
        std::vector<unsigned char> buffer(1);
        std::span<unsigned char> bufferSpan(buffer);
        std::span<unsigned char> message;
        
        // The queue should be empty (no messages) but not fail due to garbage header values
        bool result = subscriber->TryDequeue(bufferSpan, message);
        
        // If the header was not initialized, this would likely crash or return garbage
        // A properly initialized empty queue should return false (no messages available)
        EXPECT_FALSE(result) << "Empty queue should return false, not crash or return garbage data";
        EXPECT_TRUE(message.empty()) << "Empty queue should return empty message span";
    }
}

// Test that queue header values are consistent between publisher and subscriber
TEST(QueueInitializationTests, PublisherAndSubscriberSeeConsistentQueue) {
    // Create a unique queue name
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::wstring queueName = L"test-consistency-queue-" + std::to_wstring(timestamp);
    
    QueueOptions options(queueName, 1024 * 1024);
    
    {
        QueueFactory factory;
        
        // Create Publisher first to ensure queue is initialized
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        
        // Send a larger message to test multi-byte patterns
        std::vector<unsigned char> testData = {42, 43, 44, 45};  // 4 bytes: 42, 43, 44, 45
        std::span<const unsigned char> message(testData);
        
        bool enqueueResult = publisher->TryEnqueue(message);
        EXPECT_TRUE(enqueueResult) << "Should be able to enqueue to new queue";
        
        // Small delay to ensure write is complete
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Now create Subscriber after the message is written
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        // Now try to dequeue the same value
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> bufferSpan(buffer);
        std::span<unsigned char> receivedMessage;
        
        bool dequeueResult = subscriber->TryDequeue(bufferSpan, receivedMessage);
        EXPECT_TRUE(dequeueResult) << "Should be able to dequeue from queue with message";
        EXPECT_FALSE(receivedMessage.empty()) << "Should receive a non-empty message";
        EXPECT_EQ(receivedMessage.size(), 4) << "Should receive exactly 4 bytes";
        
        if (receivedMessage.size() >= 4) {
            std::cout << "Sent: [" << static_cast<int>(testData[0]) << ", " << static_cast<int>(testData[1]) 
                      << ", " << static_cast<int>(testData[2]) << ", " << static_cast<int>(testData[3]) << "]" << std::endl;
            std::cout << "Received: [" << static_cast<int>(receivedMessage[0]) << ", " << static_cast<int>(receivedMessage[1])
                      << ", " << static_cast<int>(receivedMessage[2]) << ", " << static_cast<int>(receivedMessage[3]) << "]" << std::endl;
            
            EXPECT_EQ(receivedMessage[0], testData[0]) << "First byte should match (expected " << static_cast<int>(testData[0]) << ")";
            EXPECT_EQ(receivedMessage[1], testData[1]) << "Second byte should match (expected " << static_cast<int>(testData[1]) << ")";
            EXPECT_EQ(receivedMessage[2], testData[2]) << "Third byte should match (expected " << static_cast<int>(testData[2]) << ")";
            EXPECT_EQ(receivedMessage[3], testData[3]) << "Fourth byte should match (expected " << static_cast<int>(testData[3]) << ")";
        }
    }
}