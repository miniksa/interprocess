#include "pch.h"
#include "QueueFactory.h"
#include "QueueOptions.h"
#include "IPublisher.h"
#include "ISubscriber.h"
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
        // Generate unique queue name for each test
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
    auto subscriber = CreateSubscriber(1024);

    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    EXPECT_FALSE(subscriber->TryDequeue(receiveSpan, message));
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

// ===== Circular Buffer Wrapping Tests =====

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

// ===== Data Integrity Tests =====

TEST_F(QueueTest, OddEvenPatternDetection)
{
    // Regression test: Ensure odd/even byte values are preserved
    
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    // Send messages with distinct odd/even patterns
    unsigned char oddData[] = {1, 3, 5, 7, 9, 11, 13, 15};
    unsigned char evenData[] = {0, 2, 4, 6, 8, 10, 12, 14};

    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(oddData, 8)));
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(evenData, 8)));

    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    // Receive and verify odd message
    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), 8);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(message[i] % 2, 1) << "Expected odd value at position " << i;
        EXPECT_EQ(message[i], oddData[i]);
    }

    // Receive and verify even message
    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), 8);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(message[i] % 2, 0) << "Expected even value at position " << i;
        EXPECT_EQ(message[i], evenData[i]);
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

    // Test with different message sizes
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

TEST_F(QueueTest, SingleByteMessage)
{
    // Zero-length messages may not be supported, test single-byte instead
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    // Send minimal message
    unsigned char minData[1] = {42};
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(minData, 1)));

    unsigned char receiveBuffer[10];
    std::span<unsigned char> receiveSpan(receiveBuffer, 10);
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(receiveSpan, message));
    EXPECT_EQ(message.size(), 1) << "Single-byte message should have size 1";
    EXPECT_EQ(message[0], 42);
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

TEST_F(QueueTest, MultipleSubscribersSeeSameData)
{
    // Multiple subscribers can both read messages from the queue
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

    // Subscriber2 can also read from queue (may get same or next message depending on impl)
    ASSERT_TRUE(subscriber2->TryDequeue(std::span<unsigned char>(receiveBuffer2, 10), message2));
    EXPECT_EQ(message2.size(), 4);
    // Data should be valid (either data1 or data2)
    bool isData1 = (message2[0] == 10);
    bool isData2 = (message2[0] == 50);
    EXPECT_TRUE(isData1 || isData2) << "Should receive either message 1 or 2";
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

TEST_F(QueueTest, SmallBufferWithLargeCapacity)
{
    auto publisher = CreatePublisher(8192);
    auto subscriber = CreateSubscriber(8192);

    unsigned char data[] = {99};
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data, 1)));

    unsigned char receiveBuffer[1];
    std::span<unsigned char> message;

    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 1), message));
    EXPECT_EQ(message.size(), 1);
    EXPECT_EQ(message[0], 99);
}

// ===== Edge Case Tests =====

TEST_F(QueueTest, AlternatingEnqueueDequeue)
{
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    for (int i = 0; i < 1000; ++i)
    {
        unsigned char value = static_cast<unsigned char>(i % 256);
        ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(&value, 1)))
            << "Failed at iteration " << i;

        unsigned char receiveBuffer[1];
        std::span<unsigned char> message;

        ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 1), message))
            << "Failed at iteration " << i;
        EXPECT_EQ(message.size(), 1);
        EXPECT_EQ(message[0], value) << "Mismatch at iteration " << i;
    }
}

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

// ===== Regression Tests =====

TEST_F(QueueTest, MessageAlignmentPreserved)
{
    // Ensure message alignment doesn't corrupt data
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    // Send messages with different alignments
    unsigned char data1[] = {1, 2, 3, 4, 5, 6, 7};     // 7 bytes
    unsigned char data2[] = {8, 9, 10, 11, 12, 13, 14, 15}; // 8 bytes
    unsigned char data3[] = {16, 17, 18, 19, 20, 21, 22, 23, 24}; // 9 bytes

    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data1, 7)));
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data2, 8)));
    ASSERT_TRUE(publisher->TryEnqueue(std::span<const unsigned char>(data3, 9)));

    unsigned char receiveBuffer[20];
    std::span<unsigned char> message;

    // Verify first message
    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 20), message));
    ASSERT_EQ(message.size(), 7);
    for (size_t i = 0; i < 7; ++i)
        EXPECT_EQ(message[i], data1[i]);

    // Verify second message
    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 20), message));
    ASSERT_EQ(message.size(), 8);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(message[i], data2[i]);

    // Verify third message
    ASSERT_TRUE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 20), message));
    ASSERT_EQ(message.size(), 9);
    for (size_t i = 0; i < 9; ++i)
        EXPECT_EQ(message[i], data3[i]);
}

TEST_F(QueueTest, NoGarbageDataInNewQueue)
{
    // Regression test: Ensure new queues don't contain garbage
    auto publisher = CreatePublisher(1024);
    auto subscriber = CreateSubscriber(1024);

    // Without enqueueing anything, verify dequeue returns false
    unsigned char receiveBuffer[10];
    std::span<unsigned char> message;

    EXPECT_FALSE(subscriber->TryDequeue(std::span<unsigned char>(receiveBuffer, 10), message));
    EXPECT_TRUE(message.empty());
}