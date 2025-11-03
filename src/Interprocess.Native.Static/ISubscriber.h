#pragma once

#include <chrono>
#include <span>

namespace Cloudtoid::Interprocess
{
    /// <summary>
    /// Message subscriber that subscribes to the messages published by the publisher.
    /// </summary>
    class ISubscriber
    {
    protected:
        ISubscriber() = default;
    public:
        virtual ~ISubscriber() = default;
        ISubscriber(const ISubscriber&) = default;
        ISubscriber& operator=(const ISubscriber&) = default;
        ISubscriber(ISubscriber&&) = default;
        ISubscriber& operator=(ISubscriber&&) = default;

        /// <summary>
        /// Dequeues a message from the queue if the queue is not empty. This is a non-blocking
        /// call and returns immediately. This method does not allocated memory and only populates
        /// the <paramref name="buffer"/> that is passed in. Make sure that the buffer is large
        /// enough to receive the entire message, or the message is truncated to fit the buffer.
        /// </summary>
        /// <param name="buffer">The memory buffer that is populated with the message. Make sure
        /// that the buffer is large enough to receive the entire message, or the message is
        /// truncated to fit the buffer.</param>
        /// <param name="message">The dequeued message.</param>
        /// <returns>Returns <see langword="false"/> if the queue is empty.</returns>
        virtual bool TryDequeue(std::span<unsigned char> buffer, std::span<unsigned char>& message) = 0;

        /// <summary>
         /// Dequeues a message from the queue. If the queue is empty, it *waits* for the
         /// arrival of a new message. This call is blocking until a message is received.
         /// This method does not allocated memory and only populates
         /// the <paramref name="buffer"/> that is passed in. Make sure that the buffer is large
         /// enough to receive the entire message, or the message is truncated to fit the buffer.
         /// </summary>
         /// <param name="buffer">The memory buffer that is populated with the message. Make sure
         /// that the buffer is large enough to receive the entire message, or the message is
         /// truncated to fit the buffer.</param>
        virtual std::span<unsigned char> Dequeue(std::span<unsigned char> buffer) = 0;
    };
} // namespace Cloudtoid::Interprocess
