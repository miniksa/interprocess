#include "pch.h"
#include "QueueFactory.h"
#include "QueueOptions.h"
#include <iostream>
#include <iomanip>

using namespace Cloudtoid::Interprocess;

class MemoryAccessTests : public ::testing::Test
{
protected:
    IPublisher* CreatePublisher(const QueueOptions& options)
    {
        QueueFactory factory;
        return factory.CreatePublisher(options);
    }

    ISubscriber* CreateSubscriber(const QueueOptions& options)
    {
        QueueFactory factory;
        return factory.CreateSubscriber(options);
    }

    std::string GenerateUniqueQueueName()
    {
        static int counter = 0;
        return "MemTest_" + std::to_string(++counter) + "_" + std::to_string(GetTickCount64());
    }
};

TEST_F(MemoryAccessTests, DirectMemoryAccess)
{
    std::string queueName = GenerateUniqueQueueName();
    std::wstring wQueueName(queueName.begin(), queueName.end());
    auto options = QueueOptions(wQueueName, 1024);

    auto publisher = CreatePublisher(options);
    auto subscriber = CreateSubscriber(options);

    // Get both headers
    auto pubHeader = publisher->GetHeader();
    auto subHeader = subscriber->GetHeader();

    std::cout << "\n=== Memory Analysis ===" << std::endl;
    std::cout << "Publisher header ptr: " << pubHeader << std::endl;
    std::cout << "Subscriber header ptr: " << subHeader << std::endl;
    std::cout << "Same memory? " << (pubHeader == subHeader ? "YES" : "NO") << std::endl;

    // Initialize queue if needed
    if (pubHeader->ReadOffset == 0 && pubHeader->WriteOffset == 0)
    {
        std::cout << "Initializing queue..." << std::endl;
        pubHeader->ReadOffset = 0;
        pubHeader->WriteOffset = 0;
    }

    std::cout << "Initial ReadOffset: " << pubHeader->ReadOffset << std::endl;
    std::cout << "Initial WriteOffset: " << pubHeader->WriteOffset << std::endl;

    // Directly write a pattern to the memory after the queue header
    unsigned char* basePtr = reinterpret_cast<unsigned char*>(pubHeader);
    unsigned char* dataPtr = basePtr + sizeof(QueueHeader);
    
    std::cout << "\n=== Direct Memory Write ===" << std::endl;
    std::cout << "Base ptr: " << static_cast<void*>(basePtr) << std::endl;
    std::cout << "Data ptr: " << static_cast<void*>(dataPtr) << std::endl;
    std::cout << "QueueHeader size: " << sizeof(QueueHeader) << std::endl;
    
    // Write a known pattern directly to memory
    unsigned char testPattern[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    memcpy(dataPtr, testPattern, sizeof(testPattern));
    
    std::cout << "Wrote pattern: ";
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "0x" << std::hex << static_cast<int>(testPattern[i]) << " ";
    }
    std::cout << std::dec << std::endl;

    // Read back the pattern using subscriber's view
    unsigned char* subBasePtr = reinterpret_cast<unsigned char*>(subHeader);
    unsigned char* subDataPtr = subBasePtr + sizeof(QueueHeader);
    
    std::cout << "\n=== Direct Memory Read ===" << std::endl;
    std::cout << "Sub base ptr: " << static_cast<void*>(subBasePtr) << std::endl;
    std::cout << "Sub data ptr: " << static_cast<void*>(subDataPtr) << std::endl;
    
    std::cout << "Read pattern: ";
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "0x" << std::hex << static_cast<int>(subDataPtr[i]) << " ";
    }
    std::cout << std::dec << std::endl;

    // Verify the patterns match
    bool directAccessWorks = true;
    for (int i = 0; i < 4; ++i)
    {
        if (subDataPtr[i] != testPattern[i])
        {
            directAccessWorks = false;
            std::cout << "Mismatch at byte " << i << ": wrote 0x" << std::hex 
                     << static_cast<int>(testPattern[i]) << ", read 0x" 
                     << static_cast<int>(subDataPtr[i]) << std::dec << std::endl;
        }
    }

    if (directAccessWorks)
    {
        std::cout << "✅ Direct memory access works!" << std::endl;
    }
    else
    {
        std::cout << "❌ Direct memory access failed!" << std::endl;
    }

    EXPECT_TRUE(directAccessWorks) << "Direct memory access should work if both instances use the same memory-mapped file";
    
    // Clean up
    delete publisher;
    delete subscriber;
}