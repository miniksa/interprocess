using Cloudtoid.Interprocess;

namespace Subscriber;

internal static partial class Program
{
    internal static void Main()
    {
        // Set up an optional logger factory to redirect the traces to he console

        using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
        var logger = loggerFactory.CreateLogger("Subscriber");

        // Create the queue factory. If you are not interested in tracing the internals of
        // the queue then don't pass in a loggerFactory

        var factory = new QueueFactory(loggerFactory);

        // Create a message queue subscriber

        var options = new QueueOptions(
            queueName: "sample-queue",
            capacity: 1024 * 1024);

        using var subscriber = factory.CreateSubscriber(options);

        // Dequeue messages
        var messageBuffer = new byte[1];
        var messageCount = 0;
        var lastMessageTime = DateTime.UtcNow;

        LogStart(logger);

        while (true)
        {
            if (subscriber.TryDequeue(messageBuffer, default, out var message))
            {
                messageCount++;
                lastMessageTime = DateTime.UtcNow;
                LogDequeue(logger, messageBuffer[0], messageCount);
            }
            else
            {
                // If no messages for 3 seconds, assume producer is done
                if (messageCount > 0 && (DateTime.UtcNow - lastMessageTime).TotalSeconds > 3)
                {
                    LogFinished(logger);
                    LogTotalReceived(logger, messageCount);
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
}