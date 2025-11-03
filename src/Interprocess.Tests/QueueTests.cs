namespace Cloudtoid.Interprocess.Tests;

public class QueueTests : IClassFixture<UniquePathFixture>
{
    private static readonly byte[] ByteArray1 = [100,];
    private static readonly byte[] ByteArray2 = [100, 110];
    private static readonly byte[] ByteArray3 = [100, 110, 120];
    private static readonly byte[] ByteArray50 = Enumerable.Range(1, 50).Select(i => (byte)i).ToArray();
    private readonly UniquePathFixture fixture;
    private readonly QueueFactory queueFactory;

    public QueueTests(
        UniquePathFixture fixture,
        ITestOutputHelper testOutputHelper)
    {
        this.fixture = fixture;
#pragma warning disable CA2000 // Dispose objects before losing scope
        var loggerFactory = new LoggerFactory();
        loggerFactory.AddProvider(new XunitLoggerProvider(testOutputHelper));
#pragma warning restore CA2000 // Dispose objects before losing scope
        queueFactory = new QueueFactory(loggerFactory);
    }

    [Fact]
    [TestBeforeAfter]
    public void Sample()
    {
        var message = new byte[] { 1, 2, 3 };
        var messageBuffer = new byte[3];
        CancellationToken cancellationToken = default;

        var factory = new QueueFactory();
        var options = new QueueOptions(
            queueName: "my-queue",
            capacity: 1024 * 1024);

        using var publisher = factory.CreatePublisher(options);
        publisher.TryEnqueue(message);

        options = new QueueOptions(
            queueName: "my-queue",
            capacity: 1024 * 1024);

        using var subscriber = factory.CreateSubscriber(options);
        subscriber.TryDequeue(messageBuffer, cancellationToken, out var msg);

        msg.ToArray().Should().BeEquivalentTo(message);
    }

    [Fact]
    [TestBeforeAfter]
    public void DependencyInjectionSample()
    {
        var message = new byte[] { 1, 2, 3 };
        var messageBuffer = new byte[3];
        CancellationToken cancellationToken = default;
        var services = new ServiceCollection();

        services
            .AddInterprocessQueue() // adding the queue related components
            .AddLogging(); // optionally, we can enable logging

        var serviceProvider = services.BuildServiceProvider();
        var factory = serviceProvider.GetRequiredService<IQueueFactory>();

        var options = new QueueOptions(
            queueName: "my-queue",
            capacity: 1024 * 1024);

        using var publisher = factory.CreatePublisher(options);
        publisher.TryEnqueue(message);

        options = new QueueOptions(
            queueName: "my-queue",
            capacity: 1024 * 1024);

        using var subscriber = factory.CreateSubscriber(options);
        subscriber.TryDequeue(messageBuffer, cancellationToken, out var msg);

        msg.ToArray().Should().BeEquivalentTo(message);
    }

    [Fact]
    [TestBeforeAfter]
    public void CanEnqueueAndDequeue()
    {
        using var p = CreatePublisher(24);
        using var s = CreateSubscriber(24);

        p.TryEnqueue(ByteArray3).Should().BeTrue();
        var message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray3);

        p.TryEnqueue(ByteArray3).Should().BeTrue();
        message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray3);

        p.TryEnqueue(ByteArray2).Should().BeTrue();
        message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray2);

        p.TryEnqueue(ByteArray2).Should().BeTrue();
        message = s.Dequeue(new byte[5], default);
        message.ToArray().Should().BeEquivalentTo(ByteArray2);
    }

    [Fact]
    [TestBeforeAfter]
    public void CanEnqueueDequeueWrappedMessage()
    {
        using var p = CreatePublisher(128);
        using var s = CreateSubscriber(128);

        p.TryEnqueue(ByteArray50).Should().BeTrue();
        var message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray50);

        p.TryEnqueue(ByteArray50).Should().BeTrue();
        message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray50);

        p.TryEnqueue(ByteArray50).Should().BeTrue();
        message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray50);

        p.TryEnqueue(ByteArray50).Should().BeTrue();
        message = s.Dequeue(default);
        message.ToArray().Should().BeEquivalentTo(ByteArray50);
    }

    [Fact]
    [TestBeforeAfter]
    public void CannotEnqueuePastCapacity()
    {
        using var p = CreatePublisher(24);

        p.TryEnqueue(ByteArray3).Should().BeTrue();
        p.TryEnqueue(ByteArray1).Should().BeFalse();
    }

    [Fact]
    [TestBeforeAfter]
    public void DisposeShouldNotThrow()
    {
        var p = CreatePublisher(24);
        p.TryEnqueue(ByteArray3).Should().BeTrue();

        using var s = CreateSubscriber(24);
        p.Dispose();

        s.Dequeue(default);
    }

    [Fact]
    [TestBeforeAfter]
    public void CannotReadAfterProducerIsDisposed()
    {
        var p = CreatePublisher(24);
        p.TryEnqueue(ByteArray3).Should().BeTrue();
        using (var s = CreateSubscriber(24))
            p.Dispose();

        using (CreatePublisher(24))
        using (var s = CreateSubscriber(24))
            s.TryDequeue(default, out var message).Should().BeFalse();
    }

    [Theory]
    [Repeat(10)]
    [TestBeforeAfter]
#pragma warning disable RCS1163 // Unused parameter
#pragma warning disable IDE0060 // Remove unused parameter
#pragma warning disable xUnit1026 // Theory methods should use all of their parameters
    public async Task CanDisposeQueueAsync(int i)
#pragma warning restore xUnit1026 // Theory methods should use all of their parameters
#pragma warning restore IDE0060 // Remove unused parameter
#pragma warning restore RCS1163 // Unused parameter
    {
        using var s = CreateSubscriber(1024);
        _ = Task.Run(() => s.Dequeue(default));
        await Task.Delay(200);
    }

    [Fact]
    [TestBeforeAfter]
    public void CanCircleBuffer()
    {
        using var p = CreatePublisher(1024);
        using var s = CreateSubscriber(1024);

        var message = Enumerable.Range(100, 66).Select(i => (byte)i).ToArray();

        for (var i = 0; i < 20000; i++)
        {
            p.TryEnqueue(message).Should().BeTrue();
            var result = s.Dequeue(default);
            result.ToArray().Should().BeEquivalentTo(message);
        }
    }

    [Fact]
    [TestBeforeAfter]
    public void CanRejectLargeMessages()
    {
        using (var p = CreatePublisher(24))
        using (var s = CreateSubscriber(24))
        {
            p.TryEnqueue(ByteArray3).Should().BeTrue();
            var message = s.Dequeue(default);
            message.ToArray().Should().BeEquivalentTo(ByteArray3);

            p.TryEnqueue(ByteArray3).Should().BeTrue();

            // This should fail because the queue is out of capacity
            p.TryEnqueue(ByteArray3).Should().BeFalse();

            message = s.Dequeue(default);
            message.ToArray().Should().BeEquivalentTo(ByteArray3);

            p.TryEnqueue(ByteArray3).Should().BeTrue();
            p.TryEnqueue(ByteArray3).Should().BeFalse();
        }

        using (var p = CreatePublisher(32))
        {
            p.TryEnqueue(ByteArray3).Should().BeTrue();
            p.TryEnqueue(ByteArray3).Should().BeTrue();
            p.TryEnqueue(ByteArray3).Should().BeFalse();
        }

        using (var p = CreatePublisher(32))
            p.TryEnqueue(ByteArray50).Should().BeFalse(); // failed here
    }

    [Fact]
    [TestBeforeAfter]
    public void CanRecoverIfPublisherCrashes()
    {
        // This is very complicated test that is trying to replicate a crash scenario when the publisher
        // crashes after indicating that it is writing the message but before completing the operation.

        using var dp = new DeadlockCausingPublisher(new("qn", fixture.Path, 1024), NullLoggerFactory.Instance);
        dp.TryEnqueue(ByteArray3).Should().BeTrue();

        using var p = CreatePublisher(1024);
        p.TryEnqueue(ByteArray1).Should().BeTrue();
        using var s = CreateSubscriber(1024);

        // This line should take 10 seconds to return (that is how long the timeout is set in the code)
        // After the 10 seconds expires, we should have lost all other messages that were in the queue when we started the dequeue process.
        s.TryDequeue(default, out _).Should().BeFalse();

        // But then, after this 10 seconds delay, system should fully recover and continue with new messages
        p.TryEnqueue(ByteArray1).Should().BeTrue();
        s.TryDequeue(default, out var message).Should().BeTrue();
        message.ToArray().Should().BeEquivalentTo(ByteArray1);
    }

    // ===== Data Integrity Tests (inspired by C++ suite) =====

    [Fact]
    [TestBeforeAfter]
    public void DataIntegrityAllByteValues()
    {
        // Test that all 256 possible byte values (0-255) are preserved correctly
        // This protects against encoding/decoding issues during cross-language interop
        using var p = CreatePublisher(1024 * 1024);
        using var s = CreateSubscriber(1024 * 1024);

        var allBytes = Enumerable.Range(0, 256).Select(i => (byte)i).ToArray();

        p.TryEnqueue(allBytes).Should().BeTrue();
        var received = s.Dequeue(default);

        received.Length.Should().Be(256);
        received.ToArray().Should().BeEquivalentTo(allBytes, options => options.WithStrictOrdering());
    }

    [Theory]
    [InlineData(7)] // Non-aligned size
    [InlineData(8)] // Aligned to 8 bytes
    [InlineData(9)] // Non-aligned size
    [InlineData(15)]
    [InlineData(16)]
    [InlineData(17)]
    [TestBeforeAfter]
    public void MessageAlignmentPreserved(int messageSize)
    {
        // Test that messages of various sizes (aligned and non-aligned) are preserved correctly
        // This catches bugs where alignment assumptions corrupt data
        using var p = CreatePublisher(1024 * 1024);
        using var s = CreateSubscriber(1024 * 1024);

        var message = Enumerable.Range(0, messageSize).Select(i => (byte)(i % 256)).ToArray();

        p.TryEnqueue(message).Should().BeTrue();
        var received = s.Dequeue(default);

        received.Length.Should().Be(messageSize);
        received.ToArray().Should().BeEquivalentTo(message, options => options.WithStrictOrdering());
    }

    [Fact]
    [TestBeforeAfter]
    public void OddEvenPatternDetection()
    {
        // Test that byte patterns are preserved exactly
        // Helps catch bugs where data is corrupted during transmission
        using var p = CreatePublisher(1024 * 1024);
        using var s = CreateSubscriber(1024 * 1024);

        var pattern = new byte[100];
        for (int i = 0; i < pattern.Length; i++)
            pattern[i] = (byte)(i % 2 == 0 ? 0xAA : 0x55);

        p.TryEnqueue(pattern).Should().BeTrue();
        var received = s.Dequeue(default);

        received.ToArray().Should().BeEquivalentTo(pattern, options => options.WithStrictOrdering());

        // Verify pattern is intact
        for (int i = 0; i < received.Length; i++)
        {
            received.Span[i].Should().Be(
                (byte)(i % 2 == 0 ? 0xAA : 0x55),
                $"Pattern corruption at index {i}");
        }
    }

    // ===== Circular Buffer Wrapping Tests =====

    [Fact]
    [TestBeforeAfter]
    public void CircularBufferWrapping()
    {
        // Test that the circular buffer correctly wraps around after many iterations
        // This catches off-by-one errors in offset calculations
        const int iterations = 100;
        const int messageSize = 50;
        using var p = CreatePublisher(1024); // Small buffer to force wrapping
        using var s = CreateSubscriber(1024);

        for (int i = 0; i < iterations; i++)
        {
            var message = Enumerable.Range(0, messageSize).Select(j => (byte)((i + j) % 256)).ToArray();

            p.TryEnqueue(message).Should().BeTrue($"Failed to enqueue on iteration {i}");
            var received = s.Dequeue(default);

            received.Length.Should().Be(messageSize, $"Wrong length on iteration {i}");
            received.ToArray().Should().BeEquivalentTo(
                message,
                options => options.WithStrictOrdering(),
                $"Data corruption on iteration {i}");
        }
    }

    // ===== Stress Tests =====

    [Fact]
    [TestBeforeAfter]
    public void LargeNumberOfSmallMessages()
    {
        // Stress test with many small messages to catch capacity and wrapping issues
        const int messageCount = 500;
        const int messageSize = 10;
        using var p = CreatePublisher(10 * 1024 * 1024);
        using var s = CreateSubscriber(10 * 1024 * 1024);

        for (int i = 0; i < messageCount; i++)
        {
            var message = Enumerable.Range(0, messageSize).Select(j => (byte)((i + j) % 256)).ToArray();
            p.TryEnqueue(message).Should().BeTrue($"Failed to enqueue message {i}");
        }

        for (int i = 0; i < messageCount; i++)
        {
            var received = s.Dequeue(default);
            received.Length.Should().Be(messageSize, $"Wrong length for message {i}");

            var expected = Enumerable.Range(0, messageSize).Select(j => (byte)((i + j) % 256)).ToArray();
            received.ToArray().Should().BeEquivalentTo(
                expected,
                options => options.WithStrictOrdering(),
                $"Data corruption in message {i}");
        }
    }

    // ===== Edge Cases =====

    [Fact]
    [TestBeforeAfter]
    public void MaximumMessageSize()
    {
        // Test large messages near capacity
        const int capacity = 10 * 1024 * 1024; // 10MB
        const int messageSize = 1024 * 1024; // 1MB
        using var p = CreatePublisher(capacity);
        using var s = CreateSubscriber(capacity);

        var message = new byte[messageSize];
        for (int i = 0; i < messageSize; i++)
            message[i] = (byte)(i % 256);

        p.TryEnqueue(message).Should().BeTrue();
        var received = s.Dequeue(default);

        received.Length.Should().Be(messageSize);
        received.ToArray().Should().BeEquivalentTo(message, options => options.WithStrictOrdering());
    }

    [Theory]
    [InlineData(1, 256)]
    [InlineData(7, 256)]
    [InlineData(8, 256)]
    [InlineData(16, 256)]
    [InlineData(32, 256)]
    [InlineData(64, 128)]
    [InlineData(128, 64)]
    [InlineData(256, 32)]
    [TestBeforeAfter]
    public void VaryingMessageSizes(int messageSize, int messageCount)
    {
        // Test various message size and count combinations
        using var p = CreatePublisher(10 * 1024 * 1024);
        using var s = CreateSubscriber(10 * 1024 * 1024);

        // Drain any leftover messages from previous test iterations
        while (s.TryDequeue(default, out _))
        {
            // Keep draining until empty
        }

        for (int i = 0; i < messageCount; i++)
        {
            var message = Enumerable.Range(0, messageSize).Select(j => (byte)((i + j) % 256)).ToArray();
            p.TryEnqueue(message).Should().BeTrue($"Failed to enqueue message {i}");
        }

        for (int i = 0; i < messageCount; i++)
        {
            var received = s.Dequeue(default);
            received.Length.Should().Be(messageSize);

            var expected = Enumerable.Range(0, messageSize).Select(j => (byte)((i + j) % 256)).ToArray();
            received.ToArray().Should().BeEquivalentTo(expected, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    [TestBeforeAfter]
    public async Task MultipleConcurrentPublishers_NoDataCorruptionAsync()
    {
        // Test multiple publishers writing simultaneously
        const int publisherCount = 10;
        const int messagesPerPublisher = 50;
        const int totalMessages = publisherCount * messagesPerPublisher;
        using var p = CreatePublisher(10 * 1024 * 1024);
        using var s = CreateSubscriber(10 * 1024 * 1024);

        var tasks = new Task[publisherCount];
        using var barrier = new Barrier(publisherCount);

        for (int publisherId = 0; publisherId < publisherCount; publisherId++)
        {
            int id = publisherId;
            tasks[id] = Task.Run(async () =>
            {
                barrier.SignalAndWait(); // Ensure all start simultaneously

                for (int i = 0; i < messagesPerPublisher; i++)
                {
                    // Each publisher sends unique values: publisherId * 1000 + messageIndex
                    var value = (id * 1000) + i;
                    var message = BitConverter.GetBytes(value);

                    bool enqueued = false;
                    while (!enqueued)
                    {
                        enqueued = p.TryEnqueue(message);
                        if (!enqueued)
                            await Task.Delay(1);
                    }
                }
            });
        }

        await Task.WhenAll(tasks);

        // Verify all messages received and no corruption
        var receivedValues = new HashSet<int>();
        for (int i = 0; i < totalMessages; i++)
        {
            var received = s.Dequeue(default);
            received.Length.Should().Be(sizeof(int));

            var value = BitConverter.ToInt32(received.Span);
            receivedValues.Add(value).Should().BeTrue($"Duplicate value {value} received");
        }

        receivedValues.Count.Should().Be(totalMessages);
    }

    [Fact]
    [TestBeforeAfter]
    public void MultipleSubscribersSeeSameData()
    {
        // Test that multiple subscribers share the same ReadOffset and consume messages sequentially
        // Validates that ReadOffset is properly synchronized across subscriber instances
        const int capacity = 10 * 1024 * 1024;

        using var p = CreatePublisher(capacity);
        using var s1 = CreateSubscriber(capacity);
        using var s2 = CreateSubscriber(capacity);

        // Enqueue two distinct messages
        byte[] data1 = [10, 20, 30, 40];
        byte[] data2 = [50, 60, 70, 80];

        p.TryEnqueue(data1).Should().BeTrue("Failed to enqueue first message");
        p.TryEnqueue(data2).Should().BeTrue("Failed to enqueue second message");

        // Subscriber1 reads first message
        s1.TryDequeue(default, out var message1).Should().BeTrue(
            "Subscriber1 should be able to read first message");
        message1.Length.Should().Be(4);
        message1.ToArray().Should().BeEquivalentTo(data1, options => options.WithStrictOrdering());

        // Subscriber2 reads next message (should get data2, not data1, because ReadOffset moved)
        s2.TryDequeue(default, out var message2).Should().BeTrue(
            "Subscriber2 should be able to read next message");
        message2.Length.Should().Be(4);
        message2.ToArray().Should().BeEquivalentTo(
            data2,
            options => options.WithStrictOrdering(),
            "Subscriber2 should get second message since ReadOffset is shared");

        // Verify queue is now empty
        s1.TryDequeue(default, out _).Should().BeFalse("Queue should be empty after both messages consumed");
        s2.TryDequeue(default, out _).Should().BeFalse("Queue should be empty after both messages consumed");
    }

    [Fact]
    [TestBeforeAfter]
    public void PublisherAndSubscriber_ShareSameQueueHeader()
    {
        // Critical: Verify both see same ReadOffset/WriteOffset for proper synchronization
        using var p = CreatePublisher(1024 * 1024);
        using var s = CreateSubscriber(1024 * 1024);

        // Initially both should see zero offsets
        var message = new byte[] { 1, 2, 3, 4 };
        p.TryEnqueue(message).Should().BeTrue();

        // After enqueue, subscriber should see the write
        var received = s.Dequeue(default);
        received.ToArray().Should().BeEquivalentTo(message);

        // Enqueue multiple messages
        for (int i = 0; i < 10; i++)
        {
            var msg = BitConverter.GetBytes(i);
            p.TryEnqueue(msg).Should().BeTrue();
        }

        // Dequeue half of them
        for (int i = 0; i < 5; i++)
            s.Dequeue(default);

        // Enqueue more - should use freed space
        for (int i = 0; i < 5; i++)
        {
            var msg = BitConverter.GetBytes(i + 100);
            p.TryEnqueue(msg).Should().BeTrue();
        }

        // Verify remaining messages are correct
        for (int i = 5; i < 10; i++)
        {
            var received2 = s.Dequeue(default);
            BitConverter.ToInt32(received2.Span).Should().Be(i);
        }

        for (int i = 0; i < 5; i++)
        {
            var received3 = s.Dequeue(default);
            BitConverter.ToInt32(received3.Span).Should().Be(i + 100);
        }
    }

    [Fact]
    [TestBeforeAfter]
    public void Queue_RejectsMessageWhenFull()
    {
        // Test capacity enforcement
        const int capacity = 1024;
        const int messageSize = 100;

        using var p = CreatePublisher(capacity);
        using var s = CreateSubscriber(capacity);

        var message = new byte[messageSize];
        int messagesEnqueued = 0;

        // Fill the queue
        while (p.TryEnqueue(message))
        {
            messagesEnqueued++;
            if (messagesEnqueued > 100) // Safety limit
                break;
        }

        messagesEnqueued.Should().BeGreaterThan(0, "Should have enqueued at least one message");

        // Try to enqueue one more - should fail
        p.TryEnqueue(message).Should().BeFalse("Queue should be full");

        // Dequeue one message
        _ = s.Dequeue(default);

        // Now should be able to enqueue again
        p.TryEnqueue(message).Should().BeTrue("Space should be available after dequeue");
    }

    [Fact]
    [TestBeforeAfter]
    public void Queue_CapacityFreedAfterDequeue()
    {
        // Verify space is properly reclaimed
        const int capacity = 2048;
        const int messageSize = 200;

        using var p = CreatePublisher(capacity);
        using var s = CreateSubscriber(capacity);

        var message = new byte[messageSize];

        // Fill queue
        int firstBatch = 0;
        while (p.TryEnqueue(message))
        {
            firstBatch++;
            if (firstBatch > 50) // Safety
                break;
        }

        // Should be full now
        p.TryEnqueue(message).Should().BeFalse();

        // Dequeue half
        for (int i = 0; i < firstBatch / 2; i++)
            _ = s.Dequeue(default);

        // Should be able to enqueue more now
        int secondBatch = 0;
        while (p.TryEnqueue(message))
        {
            secondBatch++;
            if (secondBatch > 50) // Safety
                break;
        }

        secondBatch.Should().BeGreaterThan(0, "Should reclaim space after dequeue");
    }

    [Fact(Timeout = 10_000)]
    [TestBeforeAfter]
    public async Task CrossInstanceQueueStateConsistencyAsync()
    {
        // Test that multiple publisher and subscriber instances can share queue state while active
        // Verifies that ReadOffset/WriteOffset are correctly synchronized across concurrent instances
        // Note: Unlike C++, C# implementation requires at least one publisher to remain alive
        // to maintain queue state - this is a known implementation characteristic
        const int messagesPerPublisher = 5;
        const int publisherCount = 3;
        const int totalMessages = messagesPerPublisher * publisherCount;
        const int capacity = 10 * 1024 * 1024;

        // Drain any leftover messages
        using (var drainSub = CreateSubscriber(capacity))
        {
            while (drainSub.TryDequeue(default, out _))
            {
                // Keep draining
            }
        }

        // Keep all publishers alive while writing
        var publishers = new List<IPublisher>();
        var allSentValues = new List<byte>();

        try
        {
            // Phase 1: Create multiple publishers and have them all write
            for (int pubIndex = 0; pubIndex < publisherCount; pubIndex++)
            {
                var publisher = CreatePublisher(capacity);
                publishers.Add(publisher);

                for (int msgIndex = 0; msgIndex < messagesPerPublisher; msgIndex++)
                {
                    byte value = (byte)(100 + (pubIndex * 10) + msgIndex);
                    allSentValues.Add(value);

                    byte[] message = [value];
                    publisher.TryEnqueue(message).Should().BeTrue(
                        $"Publisher {pubIndex}, message {msgIndex} (value {value}) failed");
                }
            }

            // Phase 2: Read with multiple subscribers while publishers are still alive
            var allReceivedValues = new List<byte>();

            for (int subIndex = 0; subIndex < publisherCount; subIndex++)
            {
                using var subscriber = CreateSubscriber(capacity);

                for (int msgIndex = 0; msgIndex < messagesPerPublisher; msgIndex++)
                {
                    subscriber.TryDequeue(default, out var received).Should().BeTrue(
                        $"Subscriber {subIndex}, message {msgIndex} failed");

                    received.Length.Should().Be(
                        1,
                        $"Subscriber {subIndex}, message {msgIndex} wrong length");

                    allReceivedValues.Add(received.Span[0]);
                }
            }

            // Phase 3: Verify all messages received in correct order
            allReceivedValues.Count.Should().Be(totalMessages);

            for (int i = 0; i < totalMessages; i++)
            {
                allReceivedValues[i].Should().Be(
                    allSentValues[i],
                    $"Position {i}: Expected {allSentValues[i]}, Got {allReceivedValues[i]}");
            }

            // Phase 4: Verify queue is empty
            using var finalSubscriber = CreateSubscriber(capacity);
            finalSubscriber.TryDequeue(default, out _).Should().BeFalse("Queue should be empty");
        }
        finally
        {
            // Clean up all publishers
            foreach (var pub in publishers)
                pub.Dispose();
        }

        // This is so we can use the Timeout attribute - this test can block forever if the implementation breaks
        await Task.CompletedTask;
    }

    [Theory]
    [InlineData(15)] // Not multiple of 8
    [InlineData(7)] // Too small
    [InlineData(0)] // Zero
    [InlineData(-1)] // Negative
    [TestBeforeAfter]
    public void QueueOptions_RejectsInvalidCapacity(long capacity)
    {
        // Should throw for invalid capacities
        var action = () => new QueueOptions("test", fixture.Path, capacity);
        action.Should().Throw<ArgumentException>();
    }

    [Fact]
    [TestBeforeAfter]
    public void QueueOptions_RequiresQueueName()
    {
        // Should throw for null/empty names
        var action1 = () => new QueueOptions(null!, fixture.Path, 1024);
        action1.Should().Throw<ArgumentException>();

        var action2 = () => new QueueOptions(string.Empty, fixture.Path, 1024);
        action2.Should().Throw<ArgumentException>();

        // Note: Whitespace-only strings are technically valid queue names
        // The underlying CheckNonEmpty only validates null and empty strings
        // A whitespace queue name would create a valid (though odd) memory-mapped file
    }

    [Fact]
    [TestBeforeAfter]
    public void MessageHeader_CorrectSize()
    {
        // Verify MessageHeader size is 8 bytes for C#/C++ interop compatibility
        unsafe
        {
            sizeof(MessageHeader).Should().Be(8, "MessageHeader must be 8 bytes for C++ interop");
        }
    }

    [Fact]
    [TestBeforeAfter]
    public void QueueHeader_CorrectSize()
    {
        // Verify QueueHeader size for C#/C++ interop compatibility
        unsafe
        {
            sizeof(QueueHeader).Should().Be(32, "QueueHeader must be 32 bytes for C++ interop");
        }
    }

    private IPublisher CreatePublisher(long capacity) =>
        queueFactory.CreatePublisher(new("qn", fixture.Path, capacity));

    private ISubscriber CreateSubscriber(long capacity) =>
        queueFactory.CreateSubscriber(new("qn", fixture.Path, capacity));

    private sealed class DeadlockCausingPublisher(QueueOptions options, ILoggerFactory loggerFactory) :
        Queue(options, loggerFactory),
        IPublisher
    {
        public unsafe bool TryEnqueue(ReadOnlySpan<byte> message)
        {
            var bodyLength = message.Length;
            var messageLength = GetPaddedMessageLength(bodyLength);
            var header = *Header;
            Header->WriteOffset = SafeIncrementMessageOffset(header.WriteOffset, messageLength);
            return true;
        }
    }
}