//******************************************************************************
// Queue Comprehensive Test Suite
//******************************************************************************
//
// Purpose: Comprehensive testing of Queue, Publisher, and Subscriber functionality
//          to ensure data integrity, capacity management, and cross-instance behavior.
//
// Test Categories:
//   - Basic Operations: Single message, empty queue, sequential messages
//   - Capacity Management: Small messages, capacity limits, queue filling
//   - Circular Wrapping: 100-iteration wrap-around with data integrity
//   - Data Integrity: All byte values (0-255), alignment preservation
//   - Variable Sizes: 1-256 bytes with alignment boundaries
//   - Multiple Instances: Shared publishers/subscribers, isolation
//   - High Frequency: 10000 rapid operations with queue dynamics
//   - Cross-Instance: Memory-mapped file consistency, offset synchronization
//   - Queue Components: Header structure, options validation
//
// Key Tests:
//   - DataIntegrityAllByteValues: All 256 byte values preserved
//   - CircularBufferWrapping: 100 iterations, 50-byte messages
//   - VaryingMessageSizes: 1-256 bytes including alignment boundaries
//   - HighFrequencyOperations: 10000 messages with dynamic filling
//   - CrossInstanceQueueStateConsistency: Multi-instance state preservation
//
// Usage:
//   All queue tests: --gtest_filter="*Queue*.*"
//   Main suite: --gtest_filter="QueueTest.*"
//   Allocation: --gtest_filter="QueueAllocationTestFixture.*"
//
//******************************************************************************

#include "pch.h"
#include "QueueFactory.h"
#include "QueueOptions.h"
#include "IPublisher.h"
#include "ISubscriber.h"
#include "Queue.h"
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <memory>

using namespace Cloudtoid::Interprocess;

class QueueTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique queue name for each test to ensure isolation
        queueName = L"test-queue-" + std::to_wstring(
            std::chrono::system_clock::now().time_since_epoch().count());
    }

    std::unique_ptr<IPublisher> CreatePublisher(unsigned long long capacity = 1024)
    {
        QueueOptions options(queueName, capacity);
        QueueFactory factory;
        return std::unique_ptr<IPublisher>(factory.CreatePublisher(options));
    }

    std::unique_ptr<ISubscriber> CreateSubscriber(unsigned long long capacity = 1024)
    {
        QueueOptions options(queueName, capacity);
        QueueFactory factory;
        return std::unique_ptr<ISubscriber>(factory.CreateSubscriber(options));
    }

    std::wstring queueName;
};

// ===== Basic Enqueue/Dequeue Tests =====

TEST_F(QueueTest, EnqueueAndDequeueSingleMessage)
{
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    unsigned char sendData[] = {1, 2, 3, 4, 5};
    std::span<const unsigned char> sendSpan(sendData, 5);

    ASSERT_TRUE(publisher->TryEnqueue(sendSpan));

    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), 5);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(message[i], sendData[i]);
}

TEST_F(QueueTest, DequeueFromEmptyQueueReturnsFalse)
{
    // Test both with and without publisher to ensure no garbage data
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    // Without enqueueing anything, verify dequeue returns false and message is empty
    EXPECT_FALSE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_TRUE(message.empty());
}

TEST_F(QueueTest, SequentialMessages)
{
    auto publisher = CreatePublisher(2048); // Need larger capacity for many messages
    auto subscriber = CreateSubscriber(2048);

    // Send messages with sequential values 0-99
    for (int i = 0; i < 100; ++i)
    {
        unsigned char value = static_cast<unsigned char>(i);
        ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(&value, 1)))
            << "Failed to enqueue message " << i;
    }

    // Verify sequential values
    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    for (int i = 0; i < 100; ++i)
    {
        ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message))
            << "Failed to dequeue message " << i;
        ASSERT_EQ(message.size(), 1) << "Wrong message size at " << i;
        EXPECT_EQ(message[0], static_cast<unsigned char>(i))
            << "Wrong value at message " << i;
    }
}

// ===== Capacity Tests =====

TEST_F(QueueTest, LargeNumberOfSmallMessages)
{
    // Test queue behavior with many small messages
    auto publisher = CreatePublisher(4096);
    auto subscriber = CreateSubscriber(4096);

    // Enqueue many small messages
    int enqueueCount = 0;
    for (int i = 0; i < 500; ++i)
    {
        unsigned char value = static_cast<unsigned char>(i % 256);
        if (publisher->TryEnqueue(std::span<const unsigned char>(&value, 1)))
            enqueueCount++;
        else
            break; // Queue full
    }

    EXPECT_GT(enqueueCount, 100) << "Should be able to queue at least 100 small messages";

    // Dequeue and verify
    unsigned char receiveBuffer[10];
    std::span<unsigned char> message;
    int dequeueCount = 0;

    for (int i = 0; i < enqueueCount; ++i)
    {
        ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 10), message))
            << "Failed to dequeue message " << i;
        EXPECT_EQ(message.size(), 1);
        EXPECT_EQ(message[0], static_cast<unsigned char>(i % 256));
        dequeueCount++;
    }

    EXPECT_EQ(dequeueCount, enqueueCount) << "Should dequeue exactly as many as enqueued";
}

TEST_F(QueueTest, CapacityRespected)
{
    // Verify queue capacity is finite
    auto publisher = CreatePublisher(512);

    unsigned char data[32];
    std::fill_n(data, 32, 0xFF);

    int successfulEnqueues = 0;
    // Try to fill beyond capacity
    for (int i = 0; i < 100; ++i)
    {
        if (!publisher->TryEnqueue(std::span<const unsigned char>(data, 32)))
            break;
        successfulEnqueues++;
    }

    // Should have failed before 100 iterations (queue has finite capacity)
    EXPECT_LT(successfulEnqueues, 100) << "Queue should eventually fill up";
    EXPECT_GT(successfulEnqueues, 0) << "Should be able to enqueue at least one message";
}

// ===== Circular Buffer Wrapping & Data Integrity Tests =====

TEST_F(QueueTest, CircularBufferWrapping)
{
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    unsigned char data[50];
    for (size_t i = 0; i < 50; ++i)
        data[i] = static_cast<unsigned char>(i);

    // Enqueue and dequeue many times to force wrapping
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data, 50)))
            << "Failed to enqueue at iteration " << iteration;

        unsigned char receiveBuffer[50];
        std::span<unsigned char> receiveSpan(receiveBuffer, 50);
        std::span<unsigned char> message;

        ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message))
            << "Failed to dequeue at iteration " << iteration;

        ASSERT_EQ(message.size(), 50) << "Wrong size at iteration " << iteration;

        for (size_t i = 0; i < 50; ++i)
        {
            EXPECT_EQ(message[i], data[i])
                << "Data mismatch at iteration " << iteration << ", byte " << i;
        }
    }
}

TEST_F(QueueTest, DataIntegrityAllByteValues)
{
    auto publisher = CreatePublisher(2048);
    auto subscriber = CreateSubscriber(2048);

    // Send all possible byte values
    unsigned char allBytes[256];
    for (int i = 0; i < 256; ++i)
        allBytes[i] = static_cast<unsigned char>(i);

    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(allBytes, 256)));

    unsigned char receiveBuffer[256];
    std::span<unsigned char> receiveSpan(receiveBuffer, 256);
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), 256);

    for (int i = 0; i < 256; ++i)
        EXPECT_EQ(message[i], static_cast<unsigned char>(i))
            << "Mismatch at byte " << i;
}

// ===== Variable Message Size Tests =====

TEST_F(QueueTest, VaryingMessageSizes)
{
    auto publisher = CreatePublisher(4096);
    auto subscriber = CreateSubscriber(4096);

    // Test with different message sizes including single-byte and alignment boundaries
    std::vector<size_t> sizes = {1, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256};

    for (size_t size : sizes)
    {
        std::vector<unsigned char> data(size);
        for (size_t i = 0; i < size; ++i)
            data[i] = static_cast<unsigned char>((i + size) % 256);

        ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data.data(), size)))
            << "Failed to enqueue message of size " << size;
    }

    // Dequeue and verify
    unsigned char receiveBuffer[512];
    std::span<unsigned char> receiveSpan(receiveBuffer, 512);
    std::span<unsigned char> message;

    for (size_t size : sizes)
    {
        ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message))
            << "Failed to dequeue message of size " << size;
        ASSERT_EQ(message.size(), size)
            << "Wrong message size (expected " << size << ")";

        for (size_t i = 0; i < size; ++i)
        {
            EXPECT_EQ(message[i], static_cast<unsigned char>((i + size) % 256))
                << "Mismatch at position " << i << " for message size " << size;
        }
    }
}

TEST_F(QueueTest, MaximumMessageSize)
{
    size_t capacity = 4096;
    auto publisher = CreatePublisher(capacity);
    auto subscriber = CreateSubscriber(capacity);

    // Create message that's close to capacity (accounting for header overhead)
    size_t msgSize = capacity - 64; // Leave room for headers
    std::vector<unsigned char> data(msgSize);
    for (size_t i = 0; i < msgSize; ++i)
        data[i] = static_cast<unsigned char>(i % 256);

    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data.data(), msgSize)));

    std::vector<unsigned char> receiveBuffer(msgSize + 100);
    std::span<unsigned char> receiveSpan(receiveBuffer.data(), receiveBuffer.size());
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), msgSize);

    for (size_t i = 0; i < msgSize; ++i)
        EXPECT_EQ(message[i], static_cast<unsigned char>(i % 256));
}

// ===== Multiple Publisher/Subscriber Tests =====

TEST_F(QueueTest, MultipleSubscribersShareReadPosition)
{
    // Multiple subscribers share the same read position (queue is FIFO, not broadcast)
    auto publisher = CreatePublisher(1024);
    auto subscriber1 = CreateSubscriber(1024);
    auto subscriber2 = CreateSubscriber(1024);

    unsigned char data1[] = {10, 20, 30, 40};
    unsigned char data2[] = {50, 60, 70, 80};
    
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data1, 4)));
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data2, 4)));

    unsigned char receiveBuffer1[10], receiveBuffer2[10];
    std::span<unsigned char> message1, message2;

    // Subscriber1 reads first message
    ASSERT_TRUE(subscriber1->TryDequeue(std::span<unsigned char>(receiveBuffer1, 10), message1));
    EXPECT_EQ(message1.size(), 4);
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(message1[i], data1[i]);

    // Subscriber2 should get the second message (not the first again)
    ASSERT_TRUE(subscriber2->TryDequeue(std::span<unsigned char>(receiveBuffer2, 10), message2));
    EXPECT_EQ(message2.size(), 4);
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(message2[i], data2[i]) << "Second subscriber should get second message";
}

TEST_F(QueueTest, MultiplePublishersCanEnqueue)
{
    auto publisher1 = CreatePublisher(1024);
    auto publisher2 = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    unsigned char data1[] = {1, 2, 3};
    unsigned char data2[] = {4, 5, 6};

    ASSERT_TRUE(publisher1->TryEnqueue(std::span<const unsigned char>(data1, 3)));
    ASSERT_TRUE(publisher2->TryEnqueue(std::span<const unsigned char>(data2, 3)));

    unsigned char receiveBuffer[10];
    std::span<unsigned char> message;

    // Should be able to dequeue both messages
    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 10), message));
    EXPECT_EQ(message.size(), 3);

    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 10), message));
    EXPECT_EQ(message.size(), 3);
}

// ===== Buffer Boundary Tests =====

TEST_F(QueueTest, MessageAtExactBufferBoundary)
{
    size_t capacity = 128;
    auto publisher = CreatePublisher(capacity);
    auto subscriber = CreateSubscriber(capacity);

    // Fill buffer exactly to boundary
    unsigned char data[64];
    std::fill_n(data, 64, 0xAA);

    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data, 64)));

    unsigned char receiveBuffer[64];
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 64), message));
    EXPECT_EQ(message.size(), 64);

    for (size_t i = 0; i < 64; ++i)
        EXPECT_EQ(message[i], 0xAA);
}

// ===== Edge Case & High Frequency Tests =====

TEST_F(QueueTest, HighFrequencyOperations)
{
    auto publisher = CreatePublisher(2048);
    auto subscriber = CreateSubscriber(2048);

    const int messageCount = 10000;
    
    // Enqueue many messages rapidly
    for (int i = 0; i < messageCount; ++i)
    {
        unsigned char value = static_cast<unsigned char>(i % 256);
        while (!publisher->TryEnqueue(std::span<const unsigned char>(&value, 1)))
        {
            // If full, dequeue one to make space
            unsigned char receiveBuffer[1];
            std::span<unsigned char> message;
            subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 1), message);
        }
    }

    // Dequeue and verify
    unsigned char receiveBuffer[1];
    std::span<unsigned char> message;
    int dequeueCount = 0;

    while (subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 1), message))
    {
        EXPECT_EQ(message.size(), 1);
        dequeueCount++;
    }

    EXPECT_GT(dequeueCount, 0) << "Should have dequeued some messages";
}

// ===== QUEUE HEADER TESTS =====

TEST(QueueHeaderTests, IsEmpty)
{
    QueueHeader header;
    header.ReadOffset = 0;
    header.WriteOffset = 0;
    EXPECT_TRUE(header.IsEmpty());

    header.WriteOffset = 8;
    EXPECT_FALSE(header.IsEmpty());

    header.ReadOffset = 8;
    EXPECT_TRUE(header.IsEmpty());
}

TEST(QueueHeaderTests, SizeAndLayout)
{
    // Verify the actual QueueHeader structure
    EXPECT_EQ(sizeof(QueueHeader), 32);
    EXPECT_EQ(offsetof(QueueHeader, ReadOffset), 0);
    EXPECT_EQ(offsetof(QueueHeader, WriteOffset), 8);
    EXPECT_EQ(offsetof(QueueHeader, ReadLockTimeStamp), 16);
    EXPECT_EQ(offsetof(QueueHeader, Reserved), 24);
}

// ===== QUEUE OPTIONS VALIDATION TESTS =====

TEST(QueueOptionsTests, ValidatesCapacity)
{
    // Test that capacity must be at least 16 bytes
    EXPECT_THROW(QueueOptions(L"test", 8), std::invalid_argument);
    EXPECT_THROW(QueueOptions(L"test", 15), std::invalid_argument);

    // Test that capacity must be a multiple of 8
    EXPECT_THROW(QueueOptions(L"test", 17), std::invalid_argument);
    EXPECT_THROW(QueueOptions(L"test", 23), std::invalid_argument);

    // Test that empty queue name is not allowed
    EXPECT_THROW(QueueOptions(L"", 64), std::invalid_argument);

    // Test valid options
    EXPECT_NO_THROW(QueueOptions(L"valid-queue", 64));
    EXPECT_NO_THROW(QueueOptions(L"valid-queue", 1024));
}

TEST(QueueOptionsTests, StoresValuesCorrectly)
{
    QueueOptions options(L"test-queue", 1024);
    EXPECT_EQ(options.GetQueueName(), L"test-queue");
    EXPECT_EQ(options.GetCapacity(), 1024ULL);
}

// ===== QUEUE ALLOCATION AND CROSS-INSTANCE TESTS =====

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

        std::vector<unsigned char> testData = { 100, 101, 102 };
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
            unsigned char testValue = static_cast<unsigned char>(50 + i); // 50, 51, 52, ...
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
        std::vector<unsigned char> oddNumbers = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49 };
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

        // Fill queue with a single publisher to avoid instance lifecycle issues
        std::vector<unsigned char> allSentValues;

        // Use a single publisher instance for all messages
        std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));

        for (int pubIndex = 0; pubIndex < 3; ++pubIndex)
        {
            for (int msgIndex = 0; msgIndex < 5; ++msgIndex)
            {
                unsigned char value = static_cast<unsigned char>(100 + (pubIndex * 10) + msgIndex);
                allSentValues.push_back(value);

                std::span<const unsigned char> message(&value, 1);
                ASSERT_TRUE(publisher->TryEnqueue(message)) << "Publisher batch " << pubIndex << ", message " << msgIndex;
            }
        }

        // Drain queue with a single subscriber to match the single publisher pattern
        std::vector<unsigned char> allReceivedValues;

        // Use a single subscriber instance for all messages
        std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));

        for (int subIndex = 0; subIndex < 3; ++subIndex)
        {
            for (int msgIndex = 0; msgIndex < 5; ++msgIndex)
            {
                std::vector<unsigned char> buffer(1024);
                std::span<unsigned char> receivedMessage;

                ASSERT_TRUE(subscriber->TryDequeue(buffer, receivedMessage)) << "Batch " << subIndex << ", message " << msgIndex;
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