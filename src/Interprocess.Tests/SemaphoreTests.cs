using Cloudtoid.Interprocess.Semaphore.Linux;
using Cloudtoid.Interprocess.Semaphore.MacOS;
using Cloudtoid.Interprocess.Semaphore.Windows;

namespace Cloudtoid.Interprocess.Tests;

public class SemaphoreTests
{
    [Fact(Platforms = Platform.Linux | Platform.FreeBSD)]
    [TestBeforeAfter]
    public void CanReleaseAndWaitLinux()
    {
        using var sem = new SemaphoreLinux("my-sem", deleteOnDispose: true);
        sem.Wait(10).Should().BeFalse();
        sem.Release();
        sem.Release();
        sem.Wait(-1).Should().BeTrue();
        sem.Wait(10).Should().BeTrue();
        sem.Wait(0).Should().BeFalse();
        sem.Wait(10).Should().BeFalse();
        sem.Release();
        sem.Wait(10).Should().BeTrue();
    }

    [Fact(Platforms = Platform.OSX)]
    [TestBeforeAfter]
    public void CanReleaseAndWaitMacOS()
    {
        using var sem = new SemaphoreMacOS("my-sem", deleteOnDispose: true);
        sem.Wait(10).Should().BeFalse();
        sem.Release();
        sem.Release();
        sem.Wait(-1).Should().BeTrue();
        sem.Wait(10).Should().BeTrue();
        sem.Wait(0).Should().BeFalse();
        sem.Wait(10).Should().BeFalse();
        sem.Release();
        sem.Wait(10).Should().BeTrue();
    }

    [Fact(Platforms = Platform.Linux | Platform.FreeBSD)]
    [TestBeforeAfter]
    public void CanCreateMultipleSemaphoresWithSameNameLinux()
    {
        using var sem1 = new SemaphoreLinux("my-sem", deleteOnDispose: true);
        using var sem2 = new SemaphoreLinux("my-sem", deleteOnDispose: false);
        sem2.Release();
        sem1.Wait(10).Should().BeTrue();
        sem1.Wait(10).Should().BeFalse();
        sem2.Wait(10).Should().BeFalse();
    }

    [Fact(Platforms = Platform.OSX)]
    [TestBeforeAfter]
    public void CanCreateMultipleSemaphoresWithSameNameMacOS()
    {
        using var sem1 = new SemaphoreMacOS("my-sem", deleteOnDispose: true);
        using var sem2 = new SemaphoreMacOS("my-sem", deleteOnDispose: false);
        sem2.Release();
        sem1.Wait(10).Should().BeTrue();
        sem1.Wait(10).Should().BeFalse();
        sem2.Wait(10).Should().BeFalse();
    }

    [Fact(Platforms = Platform.Linux | Platform.FreeBSD)]
    [TestBeforeAfter]
    public void CanReuseSameSemaphoreNameLinux()
    {
        using (var sem = new SemaphoreLinux("my-sem", deleteOnDispose: true))
        {
            sem.Wait(10).Should().BeFalse();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }

        using (var sem = new SemaphoreLinux("my-sem", deleteOnDispose: false))
        {
            sem.Wait(10).Should().BeFalse();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }

        using (var sem = new SemaphoreLinux("my-sem", deleteOnDispose: true))
        {
            sem.Wait(10).Should().BeTrue();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }
    }

    [Fact(Platforms = Platform.OSX)]
    [TestBeforeAfter]
    public void CanReuseSameSemaphoreNameMacOS()
    {
        using (var sem = new SemaphoreMacOS("my-sem", deleteOnDispose: true))
        {
            sem.Wait(10).Should().BeFalse();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }

        using (var sem = new SemaphoreMacOS("my-sem", deleteOnDispose: false))
        {
            sem.Wait(10).Should().BeFalse();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }

        using (var sem = new SemaphoreMacOS("my-sem", deleteOnDispose: true))
        {
            sem.Wait(10).Should().BeTrue();
            sem.Release();
            sem.Wait(-1).Should().BeTrue();
            sem.Release();
        }
    }

    [Fact(Platforms = Platform.Windows)]
    [TestBeforeAfter]
    public async Task Semaphore_MultipleWaiters_AllReleasedWindowsAsync()
    {
        // Test multiple threads waiting, then all get released
        const int waiterCount = 10;
        using var sem = new SemaphoreWindows("multi-wait-test");

        var tasks = new Task<bool>[waiterCount];
        using var startBarrier = new Barrier(waiterCount + 1);

        for (int i = 0; i < waiterCount; i++)
        {
            tasks[i] = Task.Run(() =>
            {
                startBarrier.SignalAndWait(); // Wait for all tasks to be ready
                return sem.Wait(5000); // 5 second timeout
            });
        }

        startBarrier.SignalAndWait(); // Release all tasks to start waiting

        // Give them a moment to start waiting
        await Task.Delay(100);

        // Release all waiters
        for (int i = 0; i < waiterCount; i++)
            sem.Release();

        var results = await Task.WhenAll(tasks);

        // All should have succeeded
        results.Should().AllSatisfy(r => r.Should().BeTrue());
    }

    [Fact(Platforms = Platform.Linux | Platform.FreeBSD)]
    [TestBeforeAfter]
    public async Task Semaphore_MultipleWaiters_AllReleasedLinuxAsync()
    {
        const int waiterCount = 10;
        using var sem = new SemaphoreLinux("multi-wait-test-linux", deleteOnDispose: true);

        var tasks = new Task<bool>[waiterCount];
        using var startBarrier = new Barrier(waiterCount + 1);

        for (int i = 0; i < waiterCount; i++)
        {
            tasks[i] = Task.Run(() =>
            {
                startBarrier.SignalAndWait();
                return sem.Wait(5000);
            });
        }

        startBarrier.SignalAndWait();
        await Task.Delay(100);

        for (int i = 0; i < waiterCount; i++)
            sem.Release();

        var results = await Task.WhenAll(tasks);
        results.Should().AllSatisfy(r => r.Should().BeTrue());
    }

    [Fact(Platforms = Platform.OSX)]
    [TestBeforeAfter]
    public async Task Semaphore_MultipleWaiters_AllReleasedMacOSAsync()
    {
        const int waiterCount = 10;
        using var sem = new SemaphoreMacOS("multi-wait-test-macos", deleteOnDispose: true);

        var tasks = new Task<bool>[waiterCount];
        using var startBarrier = new Barrier(waiterCount + 1);

        for (int i = 0; i < waiterCount; i++)
        {
            tasks[i] = Task.Run(() =>
            {
                startBarrier.SignalAndWait();
                return sem.Wait(5000);
            });
        }

        startBarrier.SignalAndWait();
        await Task.Delay(100);

        for (int i = 0; i < waiterCount; i++)
            sem.Release();

        var results = await Task.WhenAll(tasks);
        results.Should().AllSatisfy(r => r.Should().BeTrue());
    }

    [Fact(Platforms = Platform.Windows)]
    [TestBeforeAfter]
    public void Semaphore_TimeoutBehaviorWindows()
    {
        // Test wait timeout scenarios
        using var sem = new SemaphoreWindows("timeout-test");

        // Wait with immediate timeout (0) - should fail immediately
        sem.Wait(0).Should().BeFalse();

        // Wait with short timeout (100ms) - should fail after timeout
        var sw = System.Diagnostics.Stopwatch.StartNew();
        sem.Wait(100).Should().BeFalse();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeGreaterOrEqualTo(90); // Allow some tolerance

        // Release and wait with timeout - should succeed immediately
        sem.Release();
        sw.Restart();
        sem.Wait(1000).Should().BeTrue();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeLessThan(100); // Should be fast
    }

    [Fact(Platforms = Platform.Linux | Platform.FreeBSD)]
    [TestBeforeAfter]
    public void Semaphore_TimeoutBehaviorLinux()
    {
        using var sem = new SemaphoreLinux("timeout-test-linux", deleteOnDispose: true);

        sem.Wait(0).Should().BeFalse();

        var sw = System.Diagnostics.Stopwatch.StartNew();
        sem.Wait(100).Should().BeFalse();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeGreaterOrEqualTo(90);

        sem.Release();
        sw.Restart();
        sem.Wait(1000).Should().BeTrue();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeLessThan(100);
    }

    [Fact(Platforms = Platform.OSX)]
    [TestBeforeAfter]
    public void Semaphore_TimeoutBehaviorMacOS()
    {
        using var sem = new SemaphoreMacOS("timeout-test-macos", deleteOnDispose: true);

        sem.Wait(0).Should().BeFalse();

        var sw = System.Diagnostics.Stopwatch.StartNew();
        sem.Wait(100).Should().BeFalse();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeGreaterOrEqualTo(90);

        sem.Release();
        sw.Restart();
        sem.Wait(1000).Should().BeTrue();
        sw.Stop();
        sw.ElapsedMilliseconds.Should().BeLessThan(100);
    }

    [Fact(Platforms = Platform.Windows)]
    [TestBeforeAfter]
    public async Task Semaphore_StressTest_ManyReleaseAndWaitAsync()
    {
        // Stress test with many rapid release/wait operations
        const int iterations = 1000;
        using var sem = new SemaphoreWindows("stress-test");

        var producer = Task.Run(async () =>
        {
            for (int i = 0; i < iterations; i++)
            {
                sem.Release();
                if (i % 10 == 0)
                    await Task.Delay(1);
            }
        });

        var consumer = Task.Run(() =>
        {
            int consumed = 0;
            while (consumed < iterations)
            {
                if (sem.Wait(100))
                    consumed++;
            }
            return consumed;
        });

        await Task.WhenAll(producer, consumer);
        var result = await consumer;
        result.Should().Be(iterations);
    }
}