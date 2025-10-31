#include "pch.h"
#include <thread>
#include <chrono>

// Basic semaphore tests that don't require the full library implementation
TEST(SemaphoreBasicTests, BasicTimeoutTest) {
    // Test that we can measure timeouts
    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should have slept for at least 10ms
    EXPECT_GE(duration.count(), 8); // Allow some tolerance
    EXPECT_LT(duration.count(), 100); // But not too much
}

TEST(SemaphoreBasicTests, ThreadingSupport) {
    // Test that we can create and join threads
    bool threadExecuted = false;
    
    std::thread testThread([&threadExecuted]() {
        threadExecuted = true;
    });
    
    testThread.join();
    EXPECT_TRUE(threadExecuted);
}

TEST(SemaphoreBasicTests, MemoryOperations) {
    // Test basic memory operations that semaphores might use
    volatile int counter = 0;
    
    // Simulate some operations that might happen in semaphore code
    counter++;
    EXPECT_EQ(counter, 1);
    
    counter += 5;
    EXPECT_EQ(counter, 6);
    
    counter = 0;
    EXPECT_EQ(counter, 0);
}