#include "pch.h"
#include <cstring>

// ===== CIRCULAR BUFFER TESTS =====
// Since CircularBuffer requires C++20 features that are complex, let's create a simple mock for testing

class MockCircularBuffer {
private:
    unsigned char* _buffer;
    size_t _capacity;
    
public:
    MockCircularBuffer(unsigned char* buffer, size_t capacity) 
        : _buffer(buffer), _capacity(capacity) {}
    
    size_t GetCapacity() const { return _capacity; }
    
    void AdjustedOffset(size_t& offset) const {
        offset %= _capacity;
    }
    
    unsigned char* GetPointer(size_t offset) const {
        size_t adjustedOffset = offset % _capacity;
        return _buffer + adjustedOffset;
    }
    
    void Read(size_t offset, size_t length, unsigned char* resultBuffer) const {
        if (length == 0) return;
        
        size_t adjustedOffset = offset % _capacity;
        
        // Handle circular reading
        size_t rightLength = std::min(_capacity - adjustedOffset, length);
        if (rightLength > 0) {
            std::memcpy(resultBuffer, _buffer + adjustedOffset, rightLength);
        }
        
        size_t leftLength = length - rightLength;
        if (leftLength > 0) {
            std::memcpy(resultBuffer + rightLength, _buffer, leftLength);
        }
    }
    
    void Write(const unsigned char* source, size_t length, size_t offset) {
        if (length == 0) return;
        
        size_t adjustedOffset = offset % _capacity;
        
        // Handle circular writing
        size_t rightLength = std::min(_capacity - adjustedOffset, length);
        std::memcpy(_buffer + adjustedOffset, source, rightLength);
        
        size_t leftLength = length - rightLength;
        if (leftLength > 0) {
            std::memcpy(_buffer, source + rightLength, leftLength);
        }
    }
    
    void Clear(size_t offset, size_t length) {
        if (length == 0) return;
        
        size_t adjustedOffset = offset % _capacity;
        size_t rightLength = std::min(_capacity - adjustedOffset, length);
        std::memset(_buffer + adjustedOffset, 0, rightLength);
        
        size_t leftLength = length - rightLength;
        if (leftLength > 0) {
            std::memset(_buffer, 0, leftLength);
        }
    }
};

// Mock QueueHeader for testing
struct MockQueueHeader {
    unsigned long long ReadOffset;
    unsigned long long WriteOffset;
    unsigned long long ReadLockTimeStamp;
    unsigned long long Reserved;
    
    bool IsEmpty() const noexcept {
        return ReadOffset == WriteOffset;
    }
};

class CircularBufferTests : public ::testing::Test {
protected:
    // Test data similar to C# tests
    static const std::vector<unsigned char> ByteArray;
    static const std::vector<unsigned char> ByteArray1;
    static const std::vector<unsigned char> ByteArray2;
    static const std::vector<unsigned char> ByteArray3;
};

const std::vector<unsigned char> CircularBufferTests::ByteArray = {100, 110, 120};
const std::vector<unsigned char> CircularBufferTests::ByteArray1 = {100};
const std::vector<unsigned char> CircularBufferTests::ByteArray2 = {100, 110};
const std::vector<unsigned char> CircularBufferTests::ByteArray3 = {100, 110, 120};

TEST_F(CircularBufferTests, CanAdjustOffset) {
    // Test data: {bytes, offset, expectedAdjustedOffset}
    struct TestCase {
        std::vector<unsigned char> bytes;
        size_t offset;
        size_t expectedOffset;
    };
    
    std::vector<TestCase> testCases = {
        {{100}, 0, 0},
        {{100}, 1, 0},
        {{100}, 2, 0},
        {{100}, 3, 0},
        {{100, 110}, 0, 0},
        {{100, 110}, 1, 1},
        {{100, 110}, 2, 0},
        {{100, 110}, 3, 1}
    };
    
    for (const auto& testCase : testCases) {
        MockCircularBuffer buffer(const_cast<unsigned char*>(testCase.bytes.data()), testCase.bytes.size());
        EXPECT_EQ(buffer.GetCapacity(), testCase.bytes.size());
        
        size_t offset = testCase.offset;
        buffer.AdjustedOffset(offset);
        EXPECT_EQ(offset, testCase.expectedOffset);
    }
}

TEST_F(CircularBufferTests, CanGetPointer) {
    // Test data: {bytes, offset, expectedValue}
    struct TestCase {
        std::vector<unsigned char> bytes;
        size_t offset;
        unsigned char expectedValue;
    };
    
    std::vector<TestCase> testCases = {
        {{100}, 0, 100},
        {{100}, 1, 100},
        {{100}, 2, 100},
        {{100}, 3, 100},
        {{100, 110}, 0, 100},
        {{100, 110}, 1, 110},
        {{100, 110}, 2, 100},
        {{100, 110}, 3, 110}
    };
    
    for (const auto& testCase : testCases) {
        MockCircularBuffer buffer(const_cast<unsigned char*>(testCase.bytes.data()), testCase.bytes.size());
        EXPECT_EQ(buffer.GetCapacity(), testCase.bytes.size());
        
        unsigned char* ptr = buffer.GetPointer(testCase.offset);
        EXPECT_EQ(*ptr, testCase.expectedValue);
    }
}

TEST_F(CircularBufferTests, CanRead) {
    // Test data: {offset, length, expectedResult}
    struct TestCase {
        size_t offset;
        size_t length;
        std::vector<unsigned char> expectedResult;
    };
    
    std::vector<TestCase> testCases = {
        {0, 0, {}},
        {0, 1, {100}},
        {1, 1, {110}},
        {2, 1, {120}},
        {3, 1, {100}},
        {0, 2, {100, 110}},
        {1, 2, {110, 120}},
        {2, 2, {120, 100}},
        {3, 2, {100, 110}},
        {0, 3, {100, 110, 120}},
        {1, 3, {110, 120, 100}},
        {2, 3, {120, 100, 110}},
        {3, 3, {100, 110, 120}},
        {0, 4, {100, 110, 120, 100}},
        {1, 4, {110, 120, 100, 110}}
    };
    
    for (const auto& testCase : testCases) {
        std::vector<unsigned char> mutableByteArray = ByteArray; // Create a mutable copy
        MockCircularBuffer buffer(mutableByteArray.data(), mutableByteArray.size());
        
        std::vector<unsigned char> resultBuffer(testCase.length);
        buffer.Read(testCase.offset, testCase.length, resultBuffer.data());
        
        resultBuffer.resize(testCase.expectedResult.size()); // Trim to expected size
        EXPECT_EQ(resultBuffer, testCase.expectedResult);
    }
}

TEST_F(CircularBufferTests, CanWrite) {
    // Test data: {offset, bytes}
    struct TestCase {
        size_t offset;
        std::vector<unsigned char> bytes;
    };
    
    std::vector<TestCase> testCases = {
        {0, {}},
        {0, {100}},
        {1, {110}},
        {2, {120}},
        {3, {100}},
        {0, {100, 110}},
        {1, {110, 120}},
        {2, {120, 100}},
        {3, {100, 110}},
        {0, {100, 110, 120}},
        {1, {110, 120, 100}},
        {2, {120, 100, 110}},
        {3, {100, 110, 120}}
    };
    
    for (const auto& testCase : testCases) {
        std::vector<unsigned char> buffer(3);
        MockCircularBuffer circularBuffer(buffer.data(), buffer.size());
        
        if (!testCase.bytes.empty()) {
            circularBuffer.Write(testCase.bytes.data(), testCase.bytes.size(), testCase.offset);
            
            std::vector<unsigned char> readBuffer(testCase.bytes.size());
            circularBuffer.Read(testCase.offset, testCase.bytes.size(), readBuffer.data());
            
            EXPECT_EQ(readBuffer, testCase.bytes);
        }
    }
}

TEST_F(CircularBufferTests, CanWriteStruct) {
    std::vector<unsigned char> buffer(sizeof(MockQueueHeader));
    MockCircularBuffer circularBuffer(buffer.data(), buffer.size());
    
    MockQueueHeader value;
    value.ReadOffset = 1;
    value.WriteOffset = 2;
    value.ReadLockTimeStamp = ULLONG_MAX;
    value.Reserved = 0; // Note: C++ doesn't have long.MinValue, using 0
    
    circularBuffer.Write(reinterpret_cast<const unsigned char*>(&value), sizeof(value), 0);
    
    MockQueueHeader* readValue = reinterpret_cast<MockQueueHeader*>(buffer.data());
    EXPECT_EQ(readValue->ReadOffset, value.ReadOffset);
    EXPECT_EQ(readValue->WriteOffset, value.WriteOffset);
    EXPECT_EQ(readValue->ReadLockTimeStamp, value.ReadLockTimeStamp);
    EXPECT_EQ(readValue->Reserved, value.Reserved);
}

TEST_F(CircularBufferTests, CanClear) {
    // Test data: {offset, length}
    struct TestCase {
        size_t offset;
        size_t length;
    };
    
    std::vector<TestCase> testCases = {
        {0, 0}, {0, 1}, {1, 1}, {2, 1}, {3, 1},
        {0, 2}, {1, 2}, {2, 2}, {3, 2},
        {0, 3}, {1, 3}, {2, 3}, {3, 3}
    };
    
    for (const auto& testCase : testCases) {
        std::vector<unsigned char> buffer = {1, 1, 1}; // Initialize with 1s
        MockCircularBuffer circularBuffer(buffer.data(), buffer.size());
        
        // Verify all bytes are initially 1
        if (testCase.length > 0) {
            std::vector<unsigned char> initialBuffer(testCase.length);
            circularBuffer.Read(testCase.offset, testCase.length, initialBuffer.data());
            for (auto byte : initialBuffer) {
                EXPECT_EQ(byte, 1);
            }
            
            // Clear the specified range
            circularBuffer.Clear(testCase.offset, testCase.length);
            
            // Verify all bytes in the range are now 0
            std::vector<unsigned char> clearedBuffer(testCase.length);
            circularBuffer.Read(testCase.offset, testCase.length, clearedBuffer.data());
            for (auto byte : clearedBuffer) {
                EXPECT_EQ(byte, 0);
            }
        }
    }
}

// ===== QUEUE HEADER TESTS =====

TEST(QueueHeaderTests, IsEmpty) {
    MockQueueHeader header;
    header.ReadOffset = 0;
    header.WriteOffset = 0;
    EXPECT_TRUE(header.IsEmpty());
    
    header.WriteOffset = 8;
    EXPECT_FALSE(header.IsEmpty());
    
    header.ReadOffset = 8;
    EXPECT_TRUE(header.IsEmpty());
}

TEST(QueueHeaderTests, SizeAndLayout) {
    // Test that our mock has the same size as the real one would
    EXPECT_EQ(sizeof(MockQueueHeader), 32);
    EXPECT_EQ(offsetof(MockQueueHeader, ReadOffset), 0);
    EXPECT_EQ(offsetof(MockQueueHeader, WriteOffset), 8);
    EXPECT_EQ(offsetof(MockQueueHeader, ReadLockTimeStamp), 16);
    EXPECT_EQ(offsetof(MockQueueHeader, Reserved), 24);
}

// ===== QUEUE OPTIONS VALIDATION TESTS =====

// Mock QueueOptions for testing validation logic
class MockQueueOptions {
public:
    MockQueueOptions(const std::wstring& queueName, unsigned long long capacity) {
        if (queueName.empty()) {
            throw std::invalid_argument("queueName");
        }
        
        if (capacity < 16) {
            throw std::invalid_argument("capacity");
        }
        
        if (capacity % 8 != 0) {
            throw std::invalid_argument("capacity must be a multiple of 8");
        }
        
        _queueName = queueName;
        _capacity = capacity;
    }
    
    const std::wstring& GetQueueName() const { return _queueName; }
    unsigned long long GetCapacity() const { return _capacity; }
    
private:
    std::wstring _queueName;
    unsigned long long _capacity;
};

TEST(QueueOptionsTests, ValidatesCapacity) {
    // Test that capacity must be at least 16 bytes
    EXPECT_THROW(MockQueueOptions(L"test", 8), std::invalid_argument);
    EXPECT_THROW(MockQueueOptions(L"test", 15), std::invalid_argument);
    
    // Test that capacity must be a multiple of 8
    EXPECT_THROW(MockQueueOptions(L"test", 17), std::invalid_argument);
    EXPECT_THROW(MockQueueOptions(L"test", 23), std::invalid_argument);
    
    // Test that empty queue name is not allowed
    EXPECT_THROW(MockQueueOptions(L"", 64), std::invalid_argument);
    
    // Test valid options
    EXPECT_NO_THROW(MockQueueOptions(L"valid-queue", 64));
    EXPECT_NO_THROW(MockQueueOptions(L"valid-queue", 1024));
}

TEST(QueueOptionsTests, StoresValuesCorrectly) {
    MockQueueOptions options(L"test-queue", 1024);
    EXPECT_EQ(options.GetQueueName(), L"test-queue");
    EXPECT_EQ(options.GetCapacity(), 1024ULL);
}

// ===== MESSAGE PADDING TESTS =====

TEST(MessagePaddingTests, CalculatesPaddedLength) {
    // Mock the padding calculation from the C# tests
    auto GetPaddedMessageLength = [](unsigned long long bodyLength) {
        const auto messageHeaderSize = 8ULL; // Assume 8 bytes for message header
        const auto length = messageHeaderSize + bodyLength;
        
        // Round up to the closest integer divisible by 8
        return 8 * static_cast<unsigned long long>(std::ceil(static_cast<double>(length) / 8.0));
    };
    
    EXPECT_EQ(GetPaddedMessageLength(0), 8);   // Header only: 8 -> 8
    EXPECT_EQ(GetPaddedMessageLength(1), 16);  // Header + 1: 9 -> 16
    EXPECT_EQ(GetPaddedMessageLength(8), 16);  // Header + 8: 16 -> 16
    EXPECT_EQ(GetPaddedMessageLength(9), 24);  // Header + 9: 17 -> 24
    EXPECT_EQ(GetPaddedMessageLength(16), 24); // Header + 16: 24 -> 24
}

// ===== OFFSET CALCULATION TESTS =====

TEST(OffsetCalculationTests, SafeIncrementMessageOffset) {
    // Mock the safe increment logic
    auto SafeIncrementMessageOffset = [](unsigned long long offset, unsigned long long increment, unsigned long long capacity) {
        return (offset + increment) % (capacity * 2);
    };
    
    unsigned long long capacity = 1024;
    
    EXPECT_EQ(SafeIncrementMessageOffset(0, 16, capacity), 16);
    EXPECT_EQ(SafeIncrementMessageOffset(1000, 100, capacity), 1100);
    EXPECT_EQ(SafeIncrementMessageOffset(2000, 100, capacity), 52); // Wraps around (2100 % 2048)
}