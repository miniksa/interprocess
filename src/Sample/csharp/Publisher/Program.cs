using Cloudtoid.Interprocess;

namespace Publisher;

internal static partial class Program
{
    internal static async Task Main(string[] args)
    {
        // Parse command line arguments for message count and optional queue name
        int targetMessageCount = 100; // Default to 100 messages
        string queueName = "sample-queue"; // Default queue name

        if (args.Length > 0)
        {
            if (!int.TryParse(args[0], out targetMessageCount) || targetMessageCount <= 0)
            {
                Console.WriteLine("Error: Message count must be a positive integer");
                Console.WriteLine(
                    $"Usage: {System.Diagnostics.Process.GetCurrentProcess().ProcessName} [message_count] [queue_name]");
                return;
            }
        }

        if (args.Length > 1)
            queueName = args[1];

        // Set up an optional logger factory to redirect the traces to he console

        using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
        var logger = loggerFactory.CreateLogger("Publisher");

        // Create the queue factory. If you are not interested in tracing the internals of
        // the queue then don't pass in a loggerFactory

        var factory = new QueueFactory(loggerFactory);

        // Create a message queue publisher

        var options = new QueueOptions(
            queueName: queueName,
            capacity: 1024 * 1024);

        using var publisher = factory.CreatePublisher(options);

        LogStart(logger, targetMessageCount);

        // Enqueue messages
        int messageCount = 0;
        var startTime = DateTime.UtcNow;

        while (messageCount < targetMessageCount)
        {
            // Send sequential values 0-99, repeating if targetMessageCount > 100
            byte value = (byte)(messageCount % 100);

            if (publisher.TryEnqueue([value]))
            {
                messageCount++;

                // Show progress every 10 messages or at completion
                if (messageCount % 10 == 0 || messageCount == targetMessageCount)
                    LogEnqueue(logger, value, messageCount, targetMessageCount);
            }
            else
            {
                await Task.Delay(1);
            }
        }

        var endTime = DateTime.UtcNow;
        var duration = (endTime - startTime).TotalMilliseconds;
        var throughput = duration > 0 ? messageCount * 1000.0 / duration : 0.0;

        LogFinished(logger);
        LogTotalSent(logger, messageCount);
        LogThroughput(logger, throughput);
    }

    [LoggerMessage(Level = LogLevel.Information, Message = "C# Publisher starting to send {TargetCount} messages...")]
    private static partial void LogStart(ILogger logger, int targetCount);

    [LoggerMessage(Level = LogLevel.Information,
        Message = "Sent {MessageCount}/{TargetCount} messages (Current value: {Value})")]
    private static partial void LogEnqueue(ILogger logger, int value, int messageCount, int targetCount);

    [LoggerMessage(Level = LogLevel.Information, Message = "=== C# Publisher Finished ===")]
    private static partial void LogFinished(ILogger logger);

    [LoggerMessage(Level = LogLevel.Information, Message = "Total messages sent: {MessageCount}")]
    private static partial void LogTotalSent(ILogger logger, int messageCount);

    [LoggerMessage(Level = LogLevel.Information, Message = "Average throughput: {Throughput:F1} msg/s")]
    private static partial void LogThroughput(ILogger logger, double throughput);
}