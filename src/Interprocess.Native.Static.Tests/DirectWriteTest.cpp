#include "pch.h"
#include "QueueFactory.h"
#include "QueueOptions.h"
#include <iostream>
#include <iomanip>
#include <Windows.h>

using namespace Cloudtoid::Interprocess;

TEST(DirectWriteTest, VerifyDataWritten)
{
    std::string queueName = "direct_write_test";
    std::wstring wQueueName(queueName.begin(), queueName.end());
    QueueOptions options(wQueueName, 1024);
    
    QueueFactory factory;
    auto publisher = factory.CreatePublisher(options);
    
    // Send a simple message
    unsigned char testData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    std::span<const unsigned char> message(testData, 4);
    
    bool sent = publisher->TryEnqueue(message);
    ASSERT_TRUE(sent) << "Failed to send message";
    
    std::cout << "Message sent successfully" << std::endl;
    
    // Now open the same memory-mapped file and inspect what was written
    std::wstring mappedFileName = L"CT_IP_" + wQueueName;
    HANDLE hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mappedFileName.c_str());
    if (hMapFile == NULL) {
        std::wcout << L"Failed to open memory-mapped file. Name: " << mappedFileName << L", Error: " << GetLastError() << std::endl;
    }
    ASSERT_NE(hMapFile, (HANDLE)NULL) << "Failed to open memory-mapped file";
    
    void* pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT_NE(pBuf, nullptr) << "Failed to map view of file";
    
    unsigned char* memStart = static_cast<unsigned char*>(pBuf);
    
    std::cout << "\n=== Memory Dump (first 128 bytes) ===" << std::endl;
    for (int i = 0; i < 128; i += 16)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
        for (int j = 0; j < 16 && (i + j) < 128; ++j)
        {
            std::cout << std::setw(2) << static_cast<int>(memStart[i + j]) << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::dec;
    
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
}
