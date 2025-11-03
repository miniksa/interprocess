using Cloudtoid.Interprocess;

namespace Subscriber;

internal static partial class Program
{
    internal static void Main(string[] args)
    {
        // Parse command line arguments for optional queue name
        string queueName = "sample-queue"; // Default queue name

        if (args.Length > 0)
            queueName = args[0];
        // Set up an optional logger factory to redirect the traces to he console

        using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
        var logger = loggerFactory.CreateLogger("Subscriber");

        // Create the queue factory. If you are not interested in tracing the internals of
        // the queue then don't pass in a loggerFactory

        var factory = new QueueFactory(loggerFactory);

        // Create a message queue subscriber

        var options = new QueueOptions(
            queueName: queueName,
            capacity: 1024 * 1024);

        using var subscriber = factory.CreateSubscriber(options);

        // Dequeue messages
        var messageBuffer = new byte[1];
        var messageCount = 0;
        var lastMessageTime = DateTime.UtcNow;
        var sequenceError = false;

        LogStart(logger);

        while (true)
        {
            if (subscriber.TryDequeue(messageBuffer, default, out var message))
            {
                byte receivedByte = messageBuffer[0];

                // Validate sequential value (0-99, repeating)
                int expectedByte = messageCount % 100;
                if (receivedByte != expectedByte)
                {
                    // Only log first sequence error
                    if (!sequenceError)
                    {
                        LogSequenceError(logger, messageCount + 1, expectedByte, receivedByte);
                        sequenceError = true;
                    }
                }

                messageCount++;
                lastMessageTime = DateTime.UtcNow;
                LogDequeue(logger, receivedByte, messageCount);
            }
            else
            {
                // If no messages for 3 seconds, assume producer is done
                if (messageCount > 0 && (DateTime.UtcNow - lastMessageTime).TotalSeconds > 3)
                {
                    LogFinished(logger);
                    LogTotalReceived(logger, messageCount);

                    // Validate final sequence integrity
                    if (!sequenceError)
                    {
                        int maxValue = Math.Min((messageCount - 1) % 100, 99);
                        LogSequenceSuccess(logger, messageCount, maxValue);
                    }
                    else
                    {
                        LogSequenceFailure(logger);
                    }

                    break;
                }

                // Short sleep to avoid busy waiting
                Thread.Sleep(10);
            }
        }
    }

    [LoggerMessage(Level = LogLevel.Information, Message = "C# Subscriber started, waiting for messages...")]
    private static partial void LogStart(ILogger logger);

    [LoggerMessage(Level = LogLevel.Information, Message = "=== C# Subscriber Finished ===")]
    private static partial void LogFinished(ILogger logger);

    [LoggerMessage(Level = LogLevel.Information, Message = "Total messages received: {MessageCount}")]
    private static partial void LogTotalReceived(ILogger logger, int messageCount);

    [LoggerMessage(Level = LogLevel.Information, Message = "Dequeue #{MessageCount}: {Value}")]
    private static partial void LogDequeue(ILogger logger, int value, int messageCount);

    [LoggerMessage(
        Level = LogLevel.Warning,
        Message = "SEQUENCE ERROR at message {MessageNum}: expected {Expected}, got {Received}")]
    private static partial void LogSequenceError(ILogger logger, int messageNum, int expected, int received);

    [LoggerMessage(
        Level = LogLevel.Information,
        Message = "Perfect message integrity! All {MessageCount} messages received in correct sequence (0-{MaxValue})")]
    private static partial void LogSequenceSuccess(ILogger logger, int messageCount, int maxValue);

    [LoggerMessage(
        Level = LogLevel.Error,
        Message = "Sequence validation failed. Messages received out of order or with incorrect values.")]
    private static partial void LogSequenceFailure(ILogger logger);
}