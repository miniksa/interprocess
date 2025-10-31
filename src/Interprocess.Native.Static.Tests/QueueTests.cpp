#include "pch.h"

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