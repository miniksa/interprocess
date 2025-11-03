//******************************************************************************
// Semaphore Test Suite
//******************************************************************************
//
// Purpose: Testing the SemaphoreWindows implementation for cross-process
//          synchronization using Windows semaphore primitives.
//
// Test Categories:
//   - Basic Operations: Creation, release, wait
//   - Cross-Thread: Multi-threaded signaling and waiting
//   - Timeout Behavior: Wait with timeouts
//   - Error Handling: Invalid operations
//
//******************************************************************************

#include "pch.h"
#include "SemaphoreWindows.h"
#include <thread>
#include <chrono>
#include <memory>

using namespace Cloudtoid::Interprocess::Semaphore::Windows;

class SemaphoreTests : public ::testing::Test
{
protected:
    std::wstring GenerateUniqueName()
    {
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return L"sem_test_" + std::to_wstring(timestamp);
    }
};

// ===== BASIC OPERATIONS =====

TEST_F(SemaphoreTests, CanCreateSemaphore)
{
    auto name = GenerateUniqueName();
    
    // Should not throw
    EXPECT_NO_THROW({
        SemaphoreWindows sem(name);
    });
}

TEST_F(SemaphoreTests, ReleaseIncrementsSemaphore)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    // Release should succeed
    EXPECT_NO_THROW(sem.Release());
    
    // Should be able to wait immediately (non-blocking) since we released
    EXPECT_TRUE(sem.Wait(0)) << "Wait should succeed immediately after Release";
}

TEST_F(SemaphoreTests, WaitWithoutReleaseTimesOut)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    // Wait with short timeout should fail (nothing released)
    EXPECT_FALSE(sem.Wait(10)) << "Wait should timeout when no Release has been called";
}

TEST_F(SemaphoreTests, MultipleReleasesAllowMultipleWaits)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    // Release 3 times
    sem.Release();
    sem.Release();
    sem.Release();
    
    // Should be able to wait 3 times without blocking
    EXPECT_TRUE(sem.Wait(0)) << "First wait should succeed";
    EXPECT_TRUE(sem.Wait(0)) << "Second wait should succeed";
    EXPECT_TRUE(sem.Wait(0)) << "Third wait should succeed";
    
    // Fourth wait should timeout
    EXPECT_FALSE(sem.Wait(10)) << "Fourth wait should timeout";
}

// ===== CROSS-THREAD SYNCHRONIZATION =====

TEST_F(SemaphoreTests, CrossThreadSignaling)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    bool threadCompleted = false;
    
    // Start thread that waits for signal
    std::thread waiter([&]() {
        // Wait for up to 5 seconds
        bool signaled = sem.Wait(5000);
        EXPECT_TRUE(signaled) << "Thread should receive signal";
        threadCompleted = true;
    });
    
    // Give thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Release semaphore to signal the waiting thread
    sem.Release();
    
    // Wait for thread to complete
    waiter.join();
    
    EXPECT_TRUE(threadCompleted) << "Thread should have completed";
}

TEST_F(SemaphoreTests, ProducerConsumerPattern)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    const int itemCount = 5;
    std::atomic<int> itemsConsumed{0};
    
    // Consumer thread
    std::thread consumer([&]() {
        for (int i = 0; i < itemCount; ++i)
        {
            // Wait for producer to signal (up to 5 seconds per item)
            bool received = sem.Wait(5000);
            EXPECT_TRUE(received) << "Consumer should receive signal for item " << i;
            if (received)
            {
                itemsConsumed++;
            }
        }
    });
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < itemCount; ++i)
        {
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Signal consumer that item is ready
            sem.Release();
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(itemsConsumed, itemCount) << "All items should be consumed";
}

TEST_F(SemaphoreTests, MultipleThreadsWaitingForSignal)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    const int threadCount = 3;
    std::atomic<int> threadsCompleted{0};
    std::vector<std::thread> threads;
    
    // Start multiple waiting threads
    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back([&]() {
            bool signaled = sem.Wait(5000);
            EXPECT_TRUE(signaled);
            if (signaled)
            {
                threadsCompleted++;
            }
        });
    }
    
    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Release semaphore multiple times to wake all threads
    for (int i = 0; i < threadCount; ++i)
    {
        sem.Release();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Wait for all threads
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    EXPECT_EQ(threadsCompleted, threadCount) << "All threads should complete";
}

// ===== TIMEOUT BEHAVIOR =====

TEST_F(SemaphoreTests, WaitTimeoutIsAccurate)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    // Measure how long a 100ms timeout actually takes
    auto start = std::chrono::steady_clock::now();
    bool result = sem.Wait(100);
    auto duration = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    EXPECT_FALSE(result) << "Wait should timeout";
    EXPECT_GE(ms, 90) << "Timeout should be at least 90ms";
    EXPECT_LE(ms, 200) << "Timeout should be no more than 200ms (allowing for scheduling)";
}

TEST_F(SemaphoreTests, ZeroTimeoutIsNonBlocking)
{
    auto name = GenerateUniqueName();
    SemaphoreWindows sem(name);
    
    // Zero timeout should return immediately
    auto start = std::chrono::steady_clock::now();
    bool result = sem.Wait(0);
    auto duration = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    EXPECT_FALSE(result) << "Wait should fail immediately";
    EXPECT_LT(ms, 10) << "Wait(0) should return in less than 10ms";
}

// ===== CROSS-INSTANCE BEHAVIOR =====

TEST_F(SemaphoreTests, MultipleSemaphoreInstancesShareState)
{
    auto name = GenerateUniqueName();
    
    // Create two instances with same name
    SemaphoreWindows sem1(name);
    SemaphoreWindows sem2(name);
    
    // Release on first instance
    sem1.Release();
    
    // Wait on second instance should succeed
    EXPECT_TRUE(sem2.Wait(100)) << "Second instance should see release from first instance";
    
    // Another wait should timeout (only one release)
    EXPECT_FALSE(sem2.Wait(10)) << "Should timeout after consuming the single release";
}

TEST_F(SemaphoreTests, SemaphoreResetWhenAllHandlesClosed)
{
    auto name = GenerateUniqueName();
    
    // Create and release in first instance
    {
        SemaphoreWindows sem(name);
        sem.Release();
        sem.Release();
    } // Instance destroyed - semaphore is destroyed when last handle closes
    
    // Create new instance with same name - creates NEW semaphore
    {
        SemaphoreWindows sem(name);
        
        // New semaphore should be in initial state (count = 0)
        EXPECT_FALSE(sem.Wait(10)) << "New semaphore should start at count 0";
        
        // Release and verify it works
        sem.Release();
        EXPECT_TRUE(sem.Wait(0)) << "Should be able to wait after releasing new semaphore";
    }
}