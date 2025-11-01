#include "pch.h"
#include <chrono>
#include <memory>
#include <span>
#include <vector>
#include <thread>
#include <iostream>
#include <iomanip>
#include "QueueOptions.h"
#include "QueueFactory.h"
#include "IPublisher.h"
#include "ISubscriber.h"

using namespace Cloudtoid::Interprocess;

// Debug test to examine queue header values
TEST(QueueDebugTests, QueueHeaderInspection)
{
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string queueName = "debug_test_" + std::to_string(timestamp);
    
    // Convert to wide string
    std::wstring wQueueName(queueName.begin(), queueName.end());
    QueueOptions options(wQueueName, 1024 * 1024);
    
    QueueFactory factory;
    
    // Create publisher and check initial queue state
    std::unique_ptr<IPublisher> publisher(factory.CreatePublisher(options));
    
    std::cout << "\n=== Initial Queue State ===" << std::endl;
    
    // Now send a simple pattern that we can easily identify
    std::vector<unsigned char> testPattern = {0xAA, 0xBB, 0xCC, 0xDD};
    std::span<const unsigned char> message(testPattern);
    
    std::cout << "Sending pattern: ";
    for (auto byte : testPattern) {
        std::cout << "0x" << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
    
    bool sendResult = publisher->TryEnqueue(message);
    EXPECT_TRUE(sendResult) << "Should be able to send message";
    
    std::cout << "\n=== After Sending Message ===" << std::endl;
    
    // Try to receive the message
    std::unique_ptr<ISubscriber> subscriber(factory.CreateSubscriber(options));
    
    std::vector<unsigned char> buffer(1024);
    std::span<unsigned char> receivedMessage;
    
    bool receiveResult = subscriber->TryDequeue(buffer, receivedMessage);
    
    if (receiveResult)
    {
        std::cout << "Successfully received message" << std::endl;
        std::cout << "Received size: " << receivedMessage.size() << std::endl;
        
        std::cout << "Received pattern: ";
        for (size_t i = 0; i < receivedMessage.size() && i < 10; ++i) {
            std::cout << "0x" << std::hex << static_cast<int>(receivedMessage[i]) << " ";
        }
        std::cout << std::dec << std::endl;
        
        // Check if pattern matches
        bool patternMatches = (receivedMessage.size() == testPattern.size());
        if (patternMatches) {
            for (size_t i = 0; i < testPattern.size(); ++i) {
                if (receivedMessage[i] != testPattern[i]) {
                    patternMatches = false;
                    break;
                }
            }
        }
        
        if (patternMatches) {
            std::cout << "✅ Pattern matches perfectly!" << std::endl;
        } else {
            std::cout << "❌ Pattern mismatch detected" << std::endl;
            
            // Show detailed comparison
            for (size_t i = 0; i < std::max(testPattern.size(), receivedMessage.size()); ++i) {
                if (i < testPattern.size() && i < receivedMessage.size()) {
                    std::cout << "Byte " << i << ": sent 0x" << std::hex << static_cast<int>(testPattern[i])
                              << ", received 0x" << static_cast<int>(receivedMessage[i]);
                    if (testPattern[i] == receivedMessage[i]) {
                        std::cout << " ✅";
                    } else {
                        std::cout << " ❌ (diff: " << static_cast<int>(receivedMessage[i]) - static_cast<int>(testPattern[i]) << ")";
                    }
                    std::cout << std::dec << std::endl;
                } else if (i < testPattern.size()) {
                    std::cout << "Byte " << i << ": sent 0x" << std::hex << static_cast<int>(testPattern[i]) << ", received <missing>" << std::dec << std::endl;
                } else {
                    std::cout << "Byte " << i << ": sent <missing>, received 0x" << std::hex << static_cast<int>(receivedMessage[i]) << std::dec << std::endl;
                }
            }
        }
        
        EXPECT_TRUE(patternMatches) << "Received pattern should match sent pattern";
    }
    else
    {
        std::cout << "❌ Failed to receive message" << std::endl;
        FAIL() << "Should be able to receive the message";
    }
    
    std::cout << "\n=== After Receiving Message ===" << std::endl;
}