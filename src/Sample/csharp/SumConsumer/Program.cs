using System.Diagnostics;
using System.Globalization;
using Cloudtoid.Interprocess;

// C# Sum Consumer - validates that all messages from concurrent producers are received
// Usage: SumConsumer <expected_count> <expected_sum> <queue_name> [timeout_seconds] [event_name]
// Example: SumConsumer 500 124750 test-queue 30 ProducerStartEvent_123456

namespace SumConsumer;

internal static partial class Program
{
    internal static int Main(string[] args)
    {
        EventWaitHandle? eventHandle = null;
        try
        {
            if (args.Length < 3)
            {
                Console.Error.WriteLine("Usage: SumConsumer <expected_count> <expected_sum> <queue_name> "
                    + "[timeout_seconds] [event_name]");
                Console.Error.WriteLine("Example: SumConsumer 500 124750 test-queue 30 ProducerStartEvent_123456");
                return 1;
            }

            int expectedCount = int.Parse(args[0], CultureInfo.InvariantCulture);
            long expectedSum = long.Parse(args[1], CultureInfo.InvariantCulture);
            string queueName = args[2];
            int timeoutSeconds = args.Length > 3
                ? int.Parse(args[3], CultureInfo.InvariantCulture)
                : 30;
            string? eventName = args.Length > 4 ? args[4] : null;

            if (expectedCount <= 0)
            {
                Console.Error.WriteLine("Error: Expected count must be a positive integer");
                return 1;
            }

            Console.WriteLine("Sum Consumer starting...");
            Console.WriteLine($"Expected message count: {expectedCount}");
            Console.WriteLine($"Expected sum: {expectedSum}");
            Console.WriteLine($"Queue: {queueName}");
            Console.WriteLine($"Timeout: {timeoutSeconds} seconds");
            Console.WriteLine();

            const int capacity = 10 * 1024 * 1024; // 10MB for high concurrency

            var options = new QueueOptions(
                queueName: queueName,
                capacity: capacity);

            var factory = new QueueFactory();
            using var subscriber = factory.CreateSubscriber(options);

            Console.WriteLine("Connected to queue");

            // If event name provided, create and signal the event
            if (!string.IsNullOrEmpty(eventName))
            {
                Console.WriteLine($"Signaling start event: {eventName}");
                try
                {
                    eventHandle = new EventWaitHandle(
                        initialState: false,
                        mode: EventResetMode.ManualReset,
                        name: eventName);
                    eventHandle.Set();
                    Console.WriteLine("Event signaled - all producers starting now!");
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"Failed to create/signal event: {ex.Message}");
                    return 1;
                }
            }

            Console.WriteLine("Waiting for messages...");
            Console.WriteLine();

            var receivedValues = new List<int>();
            var uniqueValues = new HashSet<int>();
            int messageCount = 0;
            long actualSum = 0;

            var startTime = Stopwatch.StartNew();
            var lastMessageTime = Stopwatch.StartNew();
            var timeoutDuration = TimeSpan.FromSeconds(timeoutSeconds);

            var buffer = new byte[sizeof(int)];
            while (messageCount < expectedCount)
            {
                if (subscriber.TryDequeue(buffer, default, out var message))
                {
                    if (message.Length >= sizeof(int))
                    {
                        int intValue = BitConverter.ToInt32(message.Span);

                        receivedValues.Add(intValue);
                        uniqueValues.Add(intValue);
                        actualSum += intValue;
                        messageCount++;
                        lastMessageTime.Restart();

                        string receivedMsg = $"Received value: {intValue} "
                            + $"(message {messageCount}/{expectedCount}, running sum: {actualSum})";
                        Console.WriteLine(receivedMsg);
                    }
                }
                else
                {
                    // Check for timeout
                    if (lastMessageTime.Elapsed > timeoutDuration)
                    {
                        string timeoutMsg = "Timeout waiting for messages. "
                            + $"Received {messageCount}/{expectedCount}";
                        Console.Error.WriteLine(timeoutMsg);
                        break;
                    }

                    // Small sleep to avoid busy waiting
                    Thread.Sleep(1);
                }
            }

            startTime.Stop();

            // Print results
            Console.WriteLine();
            Console.WriteLine("=== Sum Consumer Results ===");
            Console.WriteLine($"Total runtime: {startTime.ElapsedMilliseconds} ms");
            Console.WriteLine($"Messages received: {messageCount} / {expectedCount}");
            Console.WriteLine($"Actual sum: {actualSum}");
            Console.WriteLine($"Expected sum: {expectedSum}");
            Console.WriteLine($"Unique values received: {uniqueValues.Count}");

            // Show received values (abbreviated if too many)
            if (receivedValues.Count <= 100)
            {
                Console.WriteLine($"Received values: [{string.Join(", ", receivedValues)}]");
            }
            else
            {
                Console.WriteLine($"Received values (first 50): [{string.Join(", ", receivedValues.Take(50))}]");
                var lastValues = string.Join(", ", receivedValues.Skip(receivedValues.Count - 50));
                Console.WriteLine($"Received values (last 50): [{lastValues}]");
            }

            // Validation
            bool success = messageCount == expectedCount && actualSum == expectedSum;

            if (success)
            {
                Console.WriteLine("✓ SUCCESS: All messages received with correct sum!");
                Console.WriteLine("   Perfect integrity - no data corruption or loss detected.");
                Console.WriteLine();
                return 0;
            }

            Console.WriteLine($"✗ FAILED: Expected sum {expectedSum} but got {actualSum}");
            Console.WriteLine($"   Difference: {actualSum - expectedSum}");
            Console.WriteLine();

            if (uniqueValues.Count != receivedValues.Count)
            {
                int duplicateCount = receivedValues.Count - uniqueValues.Count;
                Console.WriteLine($"⚠ Note: {duplicateCount} duplicate value(s) detected");
                Console.WriteLine("   This is expected when value ranges overlap.");
                Console.WriteLine();
            }

            return 1;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error: {ex.Message}");
            Console.Error.WriteLine(ex.StackTrace);
            return 1;
        }
        finally
        {
            eventHandle?.Dispose();
        }
    }
}