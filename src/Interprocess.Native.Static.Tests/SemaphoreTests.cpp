#include "pch.h"
#include <thread>
#include <chrono>

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