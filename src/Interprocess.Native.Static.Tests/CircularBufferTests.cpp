//******************************************************************************
// CircularBuffer Comprehensive Test Suite
//******************************************************************************
// 
// Purpose: Comprehensive testing of the CircularBuffer class to protect against
//          regressions, particularly the template span bug that was fixed.
//
// Test Count: 28 tests
//
// Categories:
//   - Basic Functionality: Constructor, pointer arithmetic, offset wrapping
//   - Write Operations: Span writes, template writes, wrapping, edge cases
//   - Read Operations: Basic reads, wrapping, truncation, large offsets
//   - Clear Operations: Basic clear, wrapping, zero-length, full buffer
//   - Round-Trip Tests: Write/read cycles with wrapping
//   - Edge Cases: Single-byte buffer, large offsets, full capacity, overwrites
//   - Regression Tests: Span template fix, offset handling consistency
//
// Key Regression Protections:
//   1. SpanNotWrittenAsObject - CRITICAL test ensuring std::span<T> is written
//      element-by-element, not as an object. Validates the requires constraint.
//   2. OffsetHandlingConsistency - Ensures offset wrapping is correct
//   3. All wrapping tests - Protect against buffer overflow and wrap-around bugs
//
// Usage:
//   Run all: --gtest_filter="CircularBufferTest.*"
//   Run specific: --gtest_filter="CircularBufferTest.SpanNotWrittenAsObject"
//
//******************************************************************************

#include "pch.h"
#include "CircularBuffer.h"
#include <vector>
#include <cstring>

using namespace Cloudtoid::Interprocess;

class CircularBufferTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Allocate test buffer
        testBuffer = new unsigned char[TEST_BUFFER_SIZE];
        std::memset(testBuffer, 0, TEST_BUFFER_SIZE);
    }

    void TearDown() override
    {
        delete[] testBuffer;
    }

    static constexpr size_t TEST_BUFFER_SIZE = 1024;
    unsigned char* testBuffer = nullptr;
};

// ===== Basic Functionality Tests =====

TEST_F(CircularBufferTest, ConstructorSetsCapacity)
{
    CircularBuffer buffer(testBuffer, TEST_BUFFER_SIZE);
    EXPECT_EQ(buffer.GetCapacity(), TEST_BUFFER_SIZE);
}

TEST_F(CircularBufferTest, GetPointerReturnsCorrectLocation)
{
    CircularBuffer buffer(testBuffer, 10);
    
    // Within bounds
    EXPECT_EQ(buffer.GetPointer(0), testBuffer);
    EXPECT_EQ(buffer.GetPointer(5), testBuffer + 5);
    EXPECT_EQ(buffer.GetPointer(9), testBuffer + 9);
    
    // Wrapping
    EXPECT_EQ(buffer.GetPointer(10), testBuffer);
    EXPECT_EQ(buffer.GetPointer(15), testBuffer + 5);
    EXPECT_EQ(buffer.GetPointer(20), testBuffer);
}

TEST_F(CircularBufferTest, AdjustedOffsetWrapsCorrectly)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned long long offset = 0;
    buffer.AdjustedOffset(offset);
    EXPECT_EQ(offset, 0);
    
    offset = 5;
    buffer.AdjustedOffset(offset);
    EXPECT_EQ(offset, 5);
    
    offset = 10;
    buffer.AdjustedOffset(offset);
    EXPECT_EQ(offset, 0);
    
    offset = 15;
    buffer.AdjustedOffset(offset);
    EXPECT_EQ(offset, 5);
    
    offset = 100;
    buffer.AdjustedOffset(offset);
    EXPECT_EQ(offset, 0);
}

// ===== Write Tests =====

TEST_F(CircularBufferTest, WriteSpanBasic)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char data[] = {1, 2, 3, 4, 5};
    std::span<const unsigned char> span(data, 5);
    
    buffer.Write(span, 0);
    
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(testBuffer[i], data[i]);
    }
}

TEST_F(CircularBufferTest, WriteSpanWrapping)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char data[] = {1, 2, 3, 4, 5};
    std::span<const unsigned char> span(data, 5);
    
    // Write starting at offset 8, should wrap to beginning
    buffer.Write(span, 8);
    
    EXPECT_EQ(testBuffer[8], 1);
    EXPECT_EQ(testBuffer[9], 2);
    EXPECT_EQ(testBuffer[0], 3);
    EXPECT_EQ(testBuffer[1], 4);
    EXPECT_EQ(testBuffer[2], 5);
}

TEST_F(CircularBufferTest, WriteSpanAtExactBoundary)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char data[] = {1, 2, 3, 4, 5};
    std::span<const unsigned char> span(data, 5);
    
    // Write starting at offset 10 (exactly at boundary)
    buffer.Write(span, 10);
    
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(testBuffer[i], data[i]);
    }
}

TEST_F(CircularBufferTest, WriteStructBasic)
{
    struct TestStruct
    {
        int a;
        double b;
        char c;
    };
    
    CircularBuffer buffer(testBuffer, sizeof(TestStruct) * 2);
    
    TestStruct original = {42, 3.14159, 'X'};
    buffer.Write(original, 0);
    
    TestStruct* read = reinterpret_cast<TestStruct*>(testBuffer);
    EXPECT_EQ(read->a, 42);
    EXPECT_DOUBLE_EQ(read->b, 3.14159);
    EXPECT_EQ(read->c, 'X');
}

TEST_F(CircularBufferTest, WriteStructWrapping)
{
    struct TestStruct
    {
        unsigned char data[8];
    };
    
    CircularBuffer buffer(testBuffer, 10);
    
    TestStruct original;
    for (int i = 0; i < 8; ++i)
        original.data[i] = static_cast<unsigned char>(i + 1);
    
    // Write starting at offset 5, should wrap
    buffer.Write(original, 5);
    
    // Check wrapped data
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(testBuffer[5 + i], i + 1);
    
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(testBuffer[i], i + 6);
}

TEST_F(CircularBufferTest, WriteEmptySpan)
{
    CircularBuffer buffer(testBuffer, 10);
    
    std::fill_n(testBuffer, 10, 0xFF);
    
    std::span<const unsigned char> emptySpan;
    buffer.Write(emptySpan, 0);
    
    // Buffer should remain unchanged
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(testBuffer[i], 0xFF);
}

// ===== Read Tests =====

TEST_F(CircularBufferTest, ReadBasic)
{
    CircularBuffer buffer(testBuffer, 10);
    
    // Initialize buffer with known data
    for (size_t i = 0; i < 10; ++i)
        testBuffer[i] = static_cast<unsigned char>(i + 1);
    
    unsigned char readBuffer[5];
    std::span<unsigned char> span(readBuffer, 5);
    
    auto result = buffer.Read(0, 5, span);
    
    EXPECT_EQ(result.size(), 5);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(result[i], i + 1);
}

TEST_F(CircularBufferTest, ReadWrapping)
{
    CircularBuffer buffer(testBuffer, 10);
    
    // Initialize buffer
    for (size_t i = 0; i < 10; ++i)
        testBuffer[i] = static_cast<unsigned char>(i);
    
    unsigned char readBuffer[5];
    std::span<unsigned char> span(readBuffer, 5);
    
    // Read starting at offset 8
    auto result = buffer.Read(8, 5, span);
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 8);
    EXPECT_EQ(result[1], 9);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 1);
    EXPECT_EQ(result[4], 2);
}

TEST_F(CircularBufferTest, ReadEmptyLength)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char readBuffer[5];
    std::span<unsigned char> span(readBuffer, 5);
    
    auto result = buffer.Read(0, 0, span);
    
    EXPECT_EQ(result.size(), 0);
}

TEST_F(CircularBufferTest, ReadTruncatesToBufferSize)
{
    CircularBuffer buffer(testBuffer, 10);
    
    for (size_t i = 0; i < 10; ++i)
        testBuffer[i] = static_cast<unsigned char>(i);
    
    unsigned char readBuffer[3];
    std::span<unsigned char> span(readBuffer, 3);
    
    // Request 5 bytes but buffer only has room for 3
    auto result = buffer.Read(0, 5, span);
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
}

TEST_F(CircularBufferTest, ReadAtOffsetBeyondCapacity)
{
    CircularBuffer buffer(testBuffer, 10);
    
    for (size_t i = 0; i < 10; ++i)
        testBuffer[i] = static_cast<unsigned char>(i);
    
    unsigned char readBuffer[3];
    std::span<unsigned char> span(readBuffer, 3);
    
    // Read at offset 15 (wraps to offset 5)
    auto result = buffer.Read(15, 3, span);
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[1], 6);
    EXPECT_EQ(result[2], 7);
}

// ===== Clear Tests =====

TEST_F(CircularBufferTest, ClearBasic)
{
    CircularBuffer buffer(testBuffer, 10);
    
    // Fill with non-zero data
    std::fill_n(testBuffer, 10, 0xFF);
    
    buffer.Clear(0, 5);
    
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(testBuffer[i], 0);
    
    for (size_t i = 5; i < 10; ++i)
        EXPECT_EQ(testBuffer[i], 0xFF);
}

TEST_F(CircularBufferTest, ClearWrapping)
{
    CircularBuffer buffer(testBuffer, 10);
    
    std::fill_n(testBuffer, 10, 0xFF);
    
    // Clear 5 bytes starting at offset 8
    buffer.Clear(8, 5);
    
    EXPECT_EQ(testBuffer[8], 0);
    EXPECT_EQ(testBuffer[9], 0);
    EXPECT_EQ(testBuffer[0], 0);
    EXPECT_EQ(testBuffer[1], 0);
    EXPECT_EQ(testBuffer[2], 0);
    
    for (size_t i = 3; i < 8; ++i)
        EXPECT_EQ(testBuffer[i], 0xFF);
}

TEST_F(CircularBufferTest, ClearZeroLength)
{
    CircularBuffer buffer(testBuffer, 10);
    
    std::fill_n(testBuffer, 10, 0xFF);
    
    buffer.Clear(0, 0);
    
    // Nothing should be cleared
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(testBuffer[i], 0xFF);
}

TEST_F(CircularBufferTest, ClearEntireBuffer)
{
    CircularBuffer buffer(testBuffer, 10);
    
    std::fill_n(testBuffer, 10, 0xFF);
    
    buffer.Clear(0, 10);
    
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(testBuffer[i], 0);
}

// ===== Round-trip Tests =====

TEST_F(CircularBufferTest, WriteAndReadRoundTrip)
{
    CircularBuffer buffer(testBuffer, 20);
    
    unsigned char writeData[] = {10, 20, 30, 40, 50};
    std::span<const unsigned char> writeSpan(writeData, 5);
    
    buffer.Write(writeSpan, 0);
    
    unsigned char readBuffer[5];
    std::span<unsigned char> readSpan(readBuffer, 5);
    
    auto result = buffer.Read(0, 5, readSpan);
    
    EXPECT_EQ(result.size(), 5);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(result[i], writeData[i]);
}

TEST_F(CircularBufferTest, WriteAndReadRoundTripWrapping)
{
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char writeData[] = {1, 2, 3, 4, 5, 6, 7};
    std::span<const unsigned char> writeSpan(writeData, 7);
    
    // Write starting at offset 7 (will wrap)
    buffer.Write(writeSpan, 7);
    
    unsigned char readBuffer[7];
    std::span<unsigned char> readSpan(readBuffer, 7);
    
    auto result = buffer.Read(7, 7, readSpan);
    
    EXPECT_EQ(result.size(), 7);
    for (size_t i = 0; i < 7; ++i)
        EXPECT_EQ(result[i], writeData[i]);
}

TEST_F(CircularBufferTest, MultipleSequentialWrites)
{
    CircularBuffer buffer(testBuffer, 100);
    
    unsigned char data1[] = {1, 2, 3};
    unsigned char data2[] = {4, 5, 6};
    unsigned char data3[] = {7, 8, 9};
    
    buffer.Write(std::span<const unsigned char>(data1, 3), 0);
    buffer.Write(std::span<const unsigned char>(data2, 3), 3);
    buffer.Write(std::span<const unsigned char>(data3, 3), 6);
    
    unsigned char readBuffer[9];
    std::span<unsigned char> readSpan(readBuffer, 9);
    
    auto result = buffer.Read(0, 9, readSpan);
    
    EXPECT_EQ(result.size(), 9);
    for (size_t i = 0; i < 9; ++i)
        EXPECT_EQ(result[i], i + 1);
}

// ===== Edge Cases and Stress Tests =====

TEST_F(CircularBufferTest, SingleByteBuffer)
{
    unsigned char singleByte = 0;
    CircularBuffer buffer(&singleByte, 1);
    
    unsigned char writeData[] = {42};
    buffer.Write(std::span<const unsigned char>(writeData, 1), 0);
    
    unsigned char readBuffer[1];
    auto result = buffer.Read(0, 1, std::span<unsigned char>(readBuffer, 1));
    
    EXPECT_EQ(result[0], 42);
}

TEST_F(CircularBufferTest, LargeOffsetWrapping)
{
    CircularBuffer buffer(testBuffer, 10);
    
    for (size_t i = 0; i < 10; ++i)
        testBuffer[i] = static_cast<unsigned char>(i);
    
    unsigned char readBuffer[3];
    std::span<unsigned char> span(readBuffer, 3);
    
    // Very large offset that wraps multiple times
    auto result = buffer.Read(1000005, 3, span);
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[1], 6);
    EXPECT_EQ(result[2], 7);
}

TEST_F(CircularBufferTest, FullBufferWriteAndRead)
{
    constexpr size_t bufferSize = 256;
    unsigned char* fullBuffer = new unsigned char[bufferSize];
    CircularBuffer buffer(fullBuffer, bufferSize);
    
    // Write full buffer
    std::vector<unsigned char> writeData(bufferSize);
    for (size_t i = 0; i < bufferSize; ++i)
        writeData[i] = static_cast<unsigned char>(i);
    
    buffer.Write(std::span<const unsigned char>(writeData), 0);
    
    // Read full buffer
    std::vector<unsigned char> readData(bufferSize);
    auto result = buffer.Read(0, bufferSize, std::span<unsigned char>(readData));
    
    EXPECT_EQ(result.size(), bufferSize);
    for (size_t i = 0; i < bufferSize; ++i)
        EXPECT_EQ(result[i], writeData[i]);
    
    delete[] fullBuffer;
}

TEST_F(CircularBufferTest, OverwritePreviousData)
{
    CircularBuffer buffer(testBuffer, 10);
    
    // First write
    unsigned char data1[] = {1, 2, 3, 4, 5};
    buffer.Write(std::span<const unsigned char>(data1, 5), 0);
    
    // Overwrite with different data
    unsigned char data2[] = {10, 20, 30};
    buffer.Write(std::span<const unsigned char>(data2, 3), 0);
    
    unsigned char readBuffer[5];
    auto result = buffer.Read(0, 5, std::span<unsigned char>(readBuffer, 5));
    
    EXPECT_EQ(result[0], 10);
    EXPECT_EQ(result[1], 20);
    EXPECT_EQ(result[2], 30);
    EXPECT_EQ(result[3], 4);  // Original data
    EXPECT_EQ(result[4], 5);  // Original data
}

TEST_F(CircularBufferTest, AlternatingWriteAndClear)
{
    CircularBuffer buffer(testBuffer, 20);
    
    unsigned char data[] = {1, 2, 3, 4, 5};
    
    buffer.Write(std::span<const unsigned char>(data, 5), 0);
    buffer.Clear(0, 5);
    
    buffer.Write(std::span<const unsigned char>(data, 5), 5);
    buffer.Clear(5, 5);
    
    buffer.Write(std::span<const unsigned char>(data, 5), 10);
    
    // Only the last write should remain
    unsigned char readBuffer[20];
    auto result = buffer.Read(0, 20, std::span<unsigned char>(readBuffer, 20));
    
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(result[i], 0);
    
    for (size_t i = 10; i < 15; ++i)
        EXPECT_EQ(result[i], data[i - 10]);
}

// ===== Regression Tests (from CircularBuffer bug fixes) =====

TEST_F(CircularBufferTest, SpanNotWrittenAsObject)
{
    // Regression test: Ensure std::span itself isn't written as an object
    // This was a bug where the template overload was matching span objects
    
    CircularBuffer buffer(testBuffer, 20);
    
    unsigned char expectedData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    std::span<const unsigned char> dataSpan(expectedData, 4);
    
    buffer.Write(dataSpan, 0);
    
    // Verify the actual data was written, not the span object
    unsigned char readBuffer[4];
    auto result = buffer.Read(0, 4, std::span<unsigned char>(readBuffer, 4));
    
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0xAA);
    EXPECT_EQ(result[1], 0xBB);
    EXPECT_EQ(result[2], 0xCC);
    EXPECT_EQ(result[3], 0xDD);
    
    // Verify we didn't write a span object by checking the next bytes are still zero
    auto nextBytes = buffer.Read(4, 16, std::span<unsigned char>(readBuffer, 4));
    for (size_t i = 0; i < std::min(size_t(4), nextBytes.size()); ++i)
        EXPECT_EQ(nextBytes[i], 0);
}

TEST_F(CircularBufferTest, OffsetHandlingConsistency)
{
    // Regression test: Ensure offset handling is consistent across operations
    
    CircularBuffer buffer(testBuffer, 10);
    
    unsigned char data[] = {1, 2, 3};
    
    // Write at various offsets and verify wrapping is consistent
    buffer.Write(std::span<const unsigned char>(data, 3), 0);
    buffer.Write(std::span<const unsigned char>(data, 3), 10);
    buffer.Write(std::span<const unsigned char>(data, 3), 20);
    
    // All writes should have gone to offset 0 due to wrapping
    unsigned char readBuffer[3];
    auto result = buffer.Read(0, 3, std::span<unsigned char>(readBuffer, 3));
    
    for (size_t i = 0; i < 3; ++i)
        EXPECT_EQ(result[i], data[i]);
}
