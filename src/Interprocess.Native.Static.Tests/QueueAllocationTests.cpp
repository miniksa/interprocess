#include "pch.h"
#include <chrono>
#include <memory>
#include <span>
#include <vector>
#include <thread>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

namespace QueueAllocationTests
{
    class QueueAllocationTestFixture : public ::testing::Test
    {
    protected:
        std::string GenerateUniqueQueueName(const std::string& prefix = "allocation_test")
        {
            auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            return prefix + "_" + std::to_string(timestamp);
        }

        QueueOptions CreateOptions(const std::string& queueName, size_t capacity = 1024 * 1024)
        {
            // Convert std::string to std::wstring
            std::wstring wQueueName(queueName.begin(), queueName.end());
            return QueueOptions(wQueueName, capacity);
        }
    };

    // Test 1: Verify same queue header is accessed by multiple Publishers
    TEST_F(QueueAllocationTestFixture, MultiplePublishersSameQueueHeaderAccess)
    {
        auto queueName = GenerateUniqueQueueName("multi_pub");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        
        // Create first publisher
        std::unique_ptr<IPublisher> publisher1(factory.CreatePublisher(options));
        
        // Send a message to advance WriteOffset
        unsigned char testData = 123;
        std::span<const unsigned char> message(&testData, 1);
        ASSERT_TRUE(publisher1->TryEnqueue(message)) << "First publisher should be able to send";
        
        // Create second publisher with same queue name
        std::unique_ptr<IPublisher> publisher2(factory.CreatePublisher(options));
        
        // Second publisher should see the updated WriteOffset from first publisher
        // Try to send another message - this should work if they share the same queue
        unsigned char testData2 = 124;
        std::span<const unsigned char> message2(&testData2, 1);
        EXPECT_TRUE(publisher2->TryEnqueue(message2)) << "Second publisher should access same queue";
        
        // Verify with subscriber that both messages are there
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        
        // Should receive first message
        ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage.size(), 1);
        
        // Should receive second message
        ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage.size(), 1);
    }

    // Test 2: Verify same queue header is accessed by multiple Subscribers
    TEST_F(QueueAllocationTestFixture, MultipleSubscribersSameQueueHeaderAccess)
    {
        auto queueName = GenerateUniqueQueueName("multi_sub");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        
        // Send multiple messages
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        
        std::vector<unsigned char> testData = {100, 101, 102};
        for (auto data : testData)
        {
            std::span<const unsigned char> message(&data, 1);
            ASSERT_TRUE(publisher->TryEnqueue(message));
        }
        
        // Create first subscriber and consume one message
        std::unique_ptr<ISubscriber> subscriber1(factory.CreateSubscriber(options));
        
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        
        ASSERT_TRUE(subscriber1->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage.size(), 1);
        
        // Create second subscriber - should see updated ReadOffset
        std::unique_ptr<ISubscriber> subscriber2(factory.CreateSubscriber(options));
        
        // Second subscriber should get the second message (not the first one again)
        ASSERT_TRUE(subscriber2->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage.size(), 1);
        
        // Third message should still be available
        EXPECT_TRUE(subscriber2->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage.size(), 1);
    }

    // Test 3: Publisher-Subscriber queue offset synchronization
    TEST_F(QueueAllocationTestFixture, PublisherSubscriberOffsetSynchronization)
    {
        auto queueName = GenerateUniqueQueueName("sync_test");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        
        // Create publisher and subscriber simultaneously
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        
        // Queue should be empty initially
        EXPECT_FALSE(subscriber->TryDequeue(buffer, receivedMessage)) << "Empty queue should return false";
        
        // Send a sequence of messages and verify each one
        for (int i = 0; i < 10; ++i)
        {
            unsigned char testValue = static_cast<unsigned char>(50 + i);  // 50, 51, 52, ...
            std::span<const unsigned char> message(&testValue, 1);
            
            ASSERT_TRUE(publisher->TryEnqueue(message)) << "Should be able to send message " << i;
            
            // Immediately try to receive it
            ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage)) << "Should be able to receive message " << i;
            ASSERT_EQ(receivedMessage.size(), 1) << "Should receive exactly 1 byte";
            EXPECT_EQ(receivedMessage[0], testValue) << "Message " << i << " data should match. Expected: " 
                      << static_cast<int>(testValue) << ", Got: " << static_cast<int>(receivedMessage[0]);
        }
    }

    // Test 4: Memory-mapped file consistency across instances
    TEST_F(QueueAllocationTestFixture, MemoryMappedFileConsistency)
    {
        auto queueName = GenerateUniqueQueueName("mmf_consistency");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        
        // Pattern: Write with one instance, read with another, repeat
        std::vector<unsigned char> sentValues;
        std::vector<unsigned char> receivedValues;
        
        for (int iteration = 0; iteration < 5; ++iteration)
        {
            // Create new publisher instance each time
            std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
            
            unsigned char testValue = static_cast<unsigned char>(70 + iteration);
            sentValues.push_back(testValue);
            std::span<const unsigned char> message(&testValue, 1);
            
            ASSERT_TRUE(publisher->TryEnqueue(message)) << "Iteration " << iteration << " send failed";
            
            // Create new subscriber instance each time 
            std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
            
            std::vector<unsigned char> buffer(1024);
            std::span<unsigned char> receivedMessage;
            
            ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage)) << "Iteration " << iteration << " receive failed";
            ASSERT_EQ(receivedMessage.size(), 1);
            
            receivedValues.push_back(receivedMessage[0]);
            
            std::cout << "Iteration " << iteration << " - Sent: " << static_cast<int>(testValue) 
                      << ", Received: " << static_cast<int>(receivedMessage[0]) << std::endl;
        }
        
        // Verify all values match
        ASSERT_EQ(sentValues.size(), receivedValues.size());
        for (size_t i = 0; i < sentValues.size(); ++i)
        {
            EXPECT_EQ(sentValues[i], receivedValues[i]) << "Mismatch at iteration " << i 
                      << " - Expected: " << static_cast<int>(sentValues[i]) 
                      << ", Got: " << static_cast<int>(receivedValues[i]);
        }
    }

    // Test 5: Queue name isolation - different names should be different queues
    TEST_F(QueueAllocationTestFixture, QueueNameIsolation)
    {
        auto queueName1 = GenerateUniqueQueueName("isolation1");
        auto queueName2 = GenerateUniqueQueueName("isolation2");
        
        auto options1 = CreateOptions(queueName1);
        auto options2 = CreateOptions(queueName2);
        
        QueueFactory factory;
        
        // Create publishers for different queues
        std::unique_ptr<IPublisher> publisher1(factory.CreatePublisher(options1));
        std::unique_ptr<IPublisher> publisher2(factory.CreatePublisher(options2));
        
        // Send different values to each queue
        unsigned char value1 = 200;
        unsigned char value2 = 201;
        
        std::span<const unsigned char> message1(&value1, 1);
        std::span<const unsigned char> message2(&value2, 1);
        
        ASSERT_TRUE(publisher1->TryEnqueue(message1));
        ASSERT_TRUE(publisher2->TryEnqueue(message2));
        
        // Create subscribers for each queue
        std::unique_ptr<ISubscriber> subscriber1(factory.CreateSubscriber(options1));
        std::unique_ptr<ISubscriber> subscriber2(factory.CreateSubscriber(options2));
        
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        
        // Each subscriber should only see messages from its own queue
        ASSERT_TRUE(subscriber1->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage[0], value1) << "Queue 1 should receive its own message";
        
        ASSERT_TRUE(subscriber2->TryDequeue(buffer, receivedMessage));
        EXPECT_EQ(receivedMessage[0], value2) << "Queue 2 should receive its own message";
        
        // Queues should be empty now
        EXPECT_FALSE(subscriber1->TryDequeue(buffer, receivedMessage)) << "Queue 1 should be empty";
        EXPECT_FALSE(subscriber2->TryDequeue(buffer, receivedMessage)) << "Queue 2 should be empty";
    }

    // Test 6: Detect the odd/even pattern issue
    TEST_F(QueueAllocationTestFixture, OddEvenPatternDetection)
    {
        auto queueName = GenerateUniqueQueueName("odd_even");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
        
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        
        // Test pattern: send odd numbers, see what we get back
        std::vector<unsigned char> oddNumbers = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49};
        std::vector<unsigned char> receivedNumbers;
        
        for (auto oddValue : oddNumbers)
        {
            std::span<const unsigned char> message(&oddValue, 1);
            
            ASSERT_TRUE(publisher->TryEnqueue(message)) << "Failed to send odd number: " << static_cast<int>(oddValue);
            ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage)) << "Failed to receive message for odd number: " << static_cast<int>(oddValue);
            ASSERT_EQ(receivedMessage.size(), 1);
            
            receivedNumbers.push_back(receivedMessage[0]);
            
            std::cout << "Sent odd: " << static_cast<int>(oddValue) 
                      << " (0x" << std::hex << static_cast<int>(oddValue) << ")"
                      << ", Received: " << std::dec << static_cast<int>(receivedMessage[0])
                      << " (0x" << std::hex << static_cast<int>(receivedMessage[0]) << ")"
                      << ", Parity: " << ((receivedMessage[0] % 2 == 0) ? "EVEN" : "ODD") << std::dec << std::endl;
        }
        
        // Analyze the pattern
        int oddReceived = 0;
        int evenReceived = 0;
        
        for (size_t i = 0; i < oddNumbers.size(); ++i)
        {
            if (receivedNumbers[i] % 2 == 0)
                evenReceived++;
            else
                oddReceived++;
            
            // The received number should match the sent number
            EXPECT_EQ(receivedNumbers[i], oddNumbers[i]) << "Mismatch at index " << i 
                      << " - Sent: " << static_cast<int>(oddNumbers[i]) 
                      << ", Received: " << static_cast<int>(receivedNumbers[i]);
        }
        
        std::cout << "Pattern analysis: Sent " << oddNumbers.size() << " odd numbers, "
                  << "Received " << oddReceived << " odd, " << evenReceived << " even" << std::endl;
        
        // All received numbers should be odd (matching what we sent)
        EXPECT_EQ(oddReceived, oddNumbers.size()) << "All received numbers should be odd since we sent odd numbers";
        EXPECT_EQ(evenReceived, 0) << "No even numbers should be received when sending odd numbers";
    }

    // Test 7: Cross-instance queue state consistency
    TEST_F(QueueAllocationTestFixture, CrossInstanceQueueStateConsistency)
    {
        auto queueName = GenerateUniqueQueueName("state_consistency");
        auto options = CreateOptions(queueName);
        
        QueueFactory factory;
        
        // Fill queue with multiple publishers
        std::vector<unsigned char> allSentValues;
        
        for (int pubIndex = 0; pubIndex < 3; ++pubIndex)
        {
            std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
            
            for (int msgIndex = 0; msgIndex < 5; ++msgIndex)
            {
                unsigned char value = static_cast<unsigned char>(100 + (pubIndex * 10) + msgIndex);
                allSentValues.push_back(value);
                
                std::span<const unsigned char> message(&value, 1);
                ASSERT_TRUE(publisher->TryEnqueue(message)) << "Publisher " << pubIndex << ", message " << msgIndex;
            }
        }
        
        // Drain queue with multiple subscribers
        std::vector<unsigned char> allReceivedValues;
        
        for (int subIndex = 0; subIndex < 3; ++subIndex)
        {
            std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
            
            for (int msgIndex = 0; msgIndex < 5; ++msgIndex)
            {
                std::vector<unsigned char> buffer(1024);
                std::span<unsigned char> receivedMessage;
                
                ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage)) << "Subscriber " << subIndex << ", message " << msgIndex;
                ASSERT_EQ(receivedMessage.size(), 1);
                
                allReceivedValues.push_back(receivedMessage[0]);
            }
        }
        
        // Verify order and values are preserved
        ASSERT_EQ(allSentValues.size(), allReceivedValues.size());
        
        for (size_t i = 0; i < allSentValues.size(); ++i)
        {
            EXPECT_EQ(allSentValues[i], allReceivedValues[i]) << "Message order/value mismatch at position " << i
                      << " - Expected: " << static_cast<int>(allSentValues[i])
                      << ", Got: " << static_cast<int>(allReceivedValues[i]);
        }
        
        // Queue should be empty now
        std::unique_ptr<ISubscriber> finalSubscriber(factory.CreateSubscriber(options));
        std::vector<unsigned char> buffer(1024);
        std::span<unsigned char> receivedMessage;
        EXPECT_FALSE(finalSubscriber->TryDequeue(buffer, receivedMessage)) << "Queue should be empty after draining";
    }
}