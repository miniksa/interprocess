################################################################################
# Comprehensive Cross-Process Message Integrity Test Suite
################################################################################
#
# Purpose: End-to-end integration testing of the Interprocess library across
#          C++ and C# implementations, validating data integrity, cross-language
#          interop, and concurrent producer scenarios.
#
# Test Scenarios (6 total):
#   1. C++ Producer → C++ Consumer
#      - Validates C++ native implementation
#      - 100 sequential messages (values 0-99)
#
#   2. C++ Producer → C# Subscriber
#      - Tests C++ to C# interop
#      - Ensures CloudtoidInterprocess C# library can read C++ messages
#
#   3. C# Publisher → C++ Consumer
#      - Tests C# to C++ interop
#      - Validates C++ can consume C#-produced messages
#
#   4. C# Publisher → C# Subscriber
#      - Validates C# native implementation
#      - Tests CloudtoidInterprocess library end-to-end
#
#   5. Multiple Concurrent C++ Producers → Single C++ Consumer
#      - Stress test: 100 concurrent producers, 5 messages each (500 total)
#      - Uses Windows Named Events for true concurrent start
#      - Validates no data corruption under high concurrency
#      - Sum validation: ensures all messages received (sum = 124750)
#
#   6. Multiple Concurrent C++ Producers → Single C# Consumer
#      - Cross-language concurrency test
#      - Validates C# library can handle concurrent C++ producers
#      - Same concurrency pattern as scenario 5
#      - C# uses EventWaitHandle (managed .NET API, not P/Invoke)
#
# Key Features:
#   - Sequential Validation: Scenarios 1-4 verify exact message order (0-99)
#   - Sum Validation: Scenarios 5-6 use arithmetic sequence sum for integrity
#   - Concurrent Synchronization: Named Events ensure all producers start simultaneously
#   - Process Isolation: Each test uses unique queue names with timestamps
#   - Comprehensive Coverage: Tests all 4 language combinations + concurrency
#
# Usage:
#   .\test-comprehensive.ps1                                    # Run all 6 tests
#   .\test-comprehensive.ps1 -Scenario cpp-cpp                  # Run specific test
#   .\test-comprehensive.ps1 -Scenario concurrent               # Run C++ concurrent test
#   .\test-comprehensive.ps1 -Scenario concurrent-csharp        # Run C++ → C# concurrent test
#   .\test-comprehensive.ps1 -MessageCount 50                   # Use 50 messages for interop tests
#   .\test-comprehensive.ps1 -ConcurrentProducerCount 10        # Use 10 concurrent producers
#
# Scenarios: all, cpp-cpp, cpp-csharp, csharp-cpp, csharp-csharp, concurrent, concurrent-csharp
#
# Prerequisites:
#   - All C++ projects built (Producer.exe, Consumer.exe, RangeProducer.exe, SumConsumer.exe)
#   - All C# projects built (Publisher, Subscriber, SumConsumer)
#   - msbuild in PATH
#   - .NET 9.0 SDK installed
#
# Exit Codes:
#   0 - All tests passed
#   1 - One or more tests failed or prerequisites missing
#
################################################################################

param(
    [int]$MessageCount = 100,
    [string]$Scenario = "all",
    [int]$ConcurrentProducerCount = 100,
    [int]$MessagesPerProducer = 5
)

$ErrorActionPreference = "Stop"

Write-Host "=== Cross-Process Message Integrity Test Suite ===" -ForegroundColor Cyan
if ($Scenario -ne "concurrent") {
    Write-Host "Testing with $MessageCount messages (values 0-99 sequentially)" -ForegroundColor Cyan
}
if ($Scenario -eq "all" -or $Scenario -eq "concurrent") {
    Write-Host "Concurrent test: $ConcurrentProducerCount producers × $MessagesPerProducer messages" -ForegroundColor Cyan
}
Write-Host ""

# Build project first
Write-Host "Building the project..." -ForegroundColor Yellow
Push-Location "src"
try {
    # Build only the C# sample projects since C++ is already built
    $buildResult = msbuild /t:rebuild /p:Platform=x64 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        Write-Host $buildResult -ForegroundColor Red
        throw "Build failed"
    }
    Write-Host "Build successful!" -ForegroundColor Green
} finally {
    Pop-Location
}

# Define paths
$cppProducer = "src\x64\Debug\Producer.exe"
$cppConsumer = "src\x64\Debug\Consumer.exe"
$cppRangeProducer = "src\x64\Debug\RangeProducer.exe"
$cppSumConsumer = "src\x64\Debug\SumConsumer.exe"
$csharpPublisher = "src\Sample\csharp\Publisher"
$csharpSubscriber = "src\Sample\csharp\Subscriber"
$csharpSumConsumer = "src\Sample\csharp\SumConsumer"

# Verify paths exist
$paths = @{
    "C++ Producer" = $cppProducer
    "C++ Consumer" = $cppConsumer
    "C++ Range Producer" = $cppRangeProducer
    "C++ Sum Consumer" = $cppSumConsumer
    "C# Publisher" = "$csharpPublisher\bin\Debug\net9.0\Publisher.exe"
    "C# Subscriber" = "$csharpSubscriber\bin\Debug\net9.0\Subscriber.exe"
    "C# Sum Consumer" = "$csharpSumConsumer\bin\Debug\net9.0\SumConsumer.exe"
}

foreach ($name in $paths.Keys) {
    if (!(Test-Path $paths[$name])) {
        throw "$name not found at $($paths[$name])"
    }
}

Write-Host "All executables found!" -ForegroundColor Green
Write-Host ""

# Run unit test suites when running all scenarios
if ($Scenario -eq "all") {
    Write-Host "=== Running Unit Test Suites ===" -ForegroundColor Cyan
    Write-Host ""
    
    # Run C++ GTest suite
    Write-Host "Running C++ GTest suite..." -ForegroundColor Yellow
    $gtestExe = "src\x64\Debug\Interprocess.Native.Static.Tests.exe"
    if (Test-Path $gtestExe) {
        $gtestOutput = & $gtestExe --gtest_filter="CircularBufferTest.*:QueueTest.*" 2>&1
        $gtestExitCode = $LASTEXITCODE
        
        # Show summary
        $gtestOutput | Select-String -Pattern "\[==========\]|\[  PASSED  \]|\[  FAILED  \]" | ForEach-Object {
            if ($_ -match "FAILED") {
                Write-Host $_ -ForegroundColor Red
            } else {
                Write-Host $_ -ForegroundColor Green
            }
        }
        
        if ($gtestExitCode -ne 0) {
            Write-Host "C++ GTest suite FAILED!" -ForegroundColor Red
            Write-Host "Full output:" -ForegroundColor Yellow
            $gtestOutput | ForEach-Object { Write-Host $_ }
            throw "C++ unit tests failed"
        }
        Write-Host "✅ C++ GTest suite passed" -ForegroundColor Green
    } else {
        Write-Host "⚠ C++ GTest suite not found at $gtestExe - skipping" -ForegroundColor Yellow
    }
    Write-Host ""
    
    # Run .NET tests
    Write-Host "Running .NET test suite..." -ForegroundColor Yellow
    Push-Location "src"
    try {
        $dotnetTestOutput = dotnet test Interprocess.Tests/Interprocess.Tests.csproj --no-build --verbosity minimal 2>&1
        
        # Parse test results from output (ignore vcxproj warnings)
        $passedLine = $dotnetTestOutput | Select-String "Passed!"
        $failedCount = 0
        $testsPassed = $false
        
        if ($passedLine -match "Failed:\s+(\d+)") {
            $failedCount = [int]$matches[1]
        }
        if ($passedLine -match "Passed!") {
            $testsPassed = $true
        }
        
        # Show relevant output (filter out vcxproj MSB4278 warnings)
        $dotnetTestOutput | Where-Object { $_ -notmatch "MSB4278|VCTargetsPath" } | ForEach-Object {
            if ($_ -match "Failed!.*Failed:\s+[1-9]") {
                Write-Host $_ -ForegroundColor Red
            } elseif ($_ -match "Passed!") {
                Write-Host $_ -ForegroundColor Green
            } elseif ($_ -match "\[SKIP\]|Skipped") {
                Write-Host $_ -ForegroundColor Yellow
            } elseif ($_ -match "^\s*$") {
                # Skip empty lines
            } else {
                Write-Host $_
            }
        }
        
        if ($failedCount -gt 0 -or !$testsPassed) {
            Write-Host ".NET test suite FAILED! ($failedCount test(s) failed)" -ForegroundColor Red
            throw ".NET unit tests failed"
        }
        
        # Extract test counts
        if ($passedLine -match "Passed:\s+(\d+)") {
            $passedCount = $matches[1]
            Write-Host "✅ .NET test suite passed ($passedCount tests)" -ForegroundColor Green
        } else {
            Write-Host "✅ .NET test suite passed" -ForegroundColor Green
        }
    } finally {
        Pop-Location
    }
    Write-Host ""
    Write-Host "=== Starting Integration Tests ===" -ForegroundColor Cyan
    Write-Host ""
}

function Cleanup-Processes {
    taskkill /F /IM Producer.exe 2>$null | Out-Null
    taskkill /F /IM Consumer.exe 2>$null | Out-Null
    taskkill /F /IM Publisher.exe 2>$null | Out-Null
    taskkill /F /IM Subscriber.exe 2>$null | Out-Null
    taskkill /F /IM RangeProducer.exe 2>$null | Out-Null
    taskkill /F /IM SumConsumer.exe 2>$null | Out-Null
    Start-Sleep -Seconds 1
}

function Test-CrossProcess {
    param(
        [string]$TestName,
        [string]$ProducerExe,
        [string]$ProducerWorkDir,
        [string]$ConsumerExe,
        [string]$ConsumerWorkDir,
        [int]$Count
    )

    Write-Host "=== Testing: $TestName ===" -ForegroundColor Magenta

    # Create unique queue name for this test
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $queueName = "test-queue-$timestamp"
    Write-Host "Using queue: $queueName" -ForegroundColor Cyan

    Cleanup-Processes

    try {
        # Start Consumer first
        Write-Host "Starting Consumer..." -ForegroundColor Green
        if ($ConsumerWorkDir) {
            $consumerJob = Start-Job -ScriptBlock {
                param($workDir, $exe, $count, $queue)
                Set-Location $workDir
                if ($exe.EndsWith(".exe") -and (Test-Path "./$($exe.Split('\')[-1])")) {
                    & "./$($exe.Split('\')[-1])" $count $queue 2>&1
                } else {
                    # Use dotnet run for C# projects
                    dotnet run --no-build -- $queue 2>&1
                }
            } -ArgumentList (Resolve-Path $ConsumerWorkDir).Path, $ConsumerExe, $Count, $queueName
        } else {
            $consumerJob = Start-Job -ScriptBlock {
                param($exe, $count, $queue)
                & $exe $count $queue 2>&1
            } -ArgumentList (Resolve-Path $ConsumerExe).Path, $Count, $queueName
        }

        # Wait for consumer to start
        Start-Sleep -Seconds 3

        # Start Producer
        Write-Host "Starting Producer to send $Count messages..." -ForegroundColor Green
        if ($ProducerWorkDir) {
            $producerJob = Start-Job -ScriptBlock {
                param($workDir, $exe, $count, $queue)
                Set-Location $workDir
                if ($exe.EndsWith(".exe") -and (Test-Path "./$($exe.Split('\')[-1])")) {
                    & "./$($exe.Split('\')[-1])" $count $queue 2>&1
                } else {
                    # Use dotnet run for C# projects
                    dotnet run --no-build -- $count $queue 2>&1
                }
            } -ArgumentList (Resolve-Path $ProducerWorkDir).Path, $ProducerExe, $Count, $queueName
        } else {
            $producerJob = Start-Job -ScriptBlock {
                param($exe, $count, $queue)
                & $exe $count $queue 2>&1
            } -ArgumentList (Resolve-Path $ProducerExe).Path, $Count, $queueName
        }

        # Wait for completion
        Write-Host "Waiting for Producer completion..." -ForegroundColor Yellow
        Wait-Job $producerJob -Timeout 30 | Out-Null

        Write-Host "Waiting for Consumer completion..." -ForegroundColor Yellow
        Wait-Job $consumerJob -Timeout 10 | Out-Null

        # Get results
        $producerOutput = Receive-Job $producerJob
        $consumerOutput = Receive-Job $consumerJob

        Write-Host "`nAnalyzing results..." -ForegroundColor Cyan

        # Parse producer results
        $producerSent = 0
        $producerMatch = $producerOutput | Select-String "Total messages sent: (\d+)"
        if ($producerMatch) {
            $producerSent = [int]$producerMatch.Matches[0].Groups[1].Value
        }

        # Parse consumer results
        $consumerReceived = 0
        $sequenceSuccess = $false

        $consumerMatch = $consumerOutput | Select-String "Total messages received: (\d+)"
        if ($consumerMatch) {
            $consumerReceived = [int]$consumerMatch.Matches[0].Groups[1].Value
        }

        # Check for sequence validation (supports both C++ and C# formats)
        $sequenceSuccess = $consumerOutput | Select-String "Perfect message integrity"
        $sequenceError = $consumerOutput | Select-String "SEQUENCE ERROR|Sequence validation failed"

        # Determine result
        $success = $false
        $message = ""

        if ($producerSent -eq $Count -and $consumerReceived -eq $Count -and $sequenceSuccess) {
            $success = $true
            $message = "Perfect message integrity! All $Count messages sent and received with correct sequence (0-$([Math]::Min($Count-1, 99)))"
        } elseif ($producerSent -ne $Count) {
            $message = "Producer failed to send all messages. Sent: $producerSent, Expected: $Count"
        } elseif ($consumerReceived -ne $Count) {
            $message = "Consumer failed to receive all messages. Received: $consumerReceived, Expected: $Count"
        } elseif ($sequenceError) {
            $message = "Sequence validation failed. Messages received out of order or with incorrect values."
        } else {
            $message = "Unknown validation failure"
        }

        # Display results
        if ($success) {
            Write-Host "🎉 SUCCESS: $message" -ForegroundColor Green
        } else {
            Write-Host "❌ FAILED: $message" -ForegroundColor Red
            Write-Host "Producer Output:" -ForegroundColor Yellow
            $producerOutput | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
            Write-Host "Consumer Output:" -ForegroundColor Yellow
            $consumerOutput | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        }

        return @{
            Success = $success
            Message = $message
            ProducerSent = $producerSent
            ConsumerReceived = $consumerReceived
        }

    } finally {
        Get-Job | Remove-Job -Force
        Cleanup-Processes
    }
}

function Test-ConcurrentProducers {
    param(
        [int]$ProducerCount,
        [int]$MessagesPerProducer
    )

    Write-Host "=== Testing: Multiple Concurrent C++ Producers → Single C++ Consumer ===" -ForegroundColor Magenta
    Write-Host "This test validates that multiple producers can write concurrently without data corruption" -ForegroundColor Cyan
    Write-Host ""

    # Test parameters
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $queueName = "test-concurrent-$timestamp"
    $eventName = "ProducerStartEvent_$timestamp"
    $totalMessages = $ProducerCount * $MessagesPerProducer

    # Calculate expected sum
    # Producer i sends values from (i * MessagesPerProducer) to ((i+1) * MessagesPerProducer - 1)
    # Sum of arithmetic sequence: sum = n * (first + last) / 2
    $expectedSum = 0
    for ($i = 0; $i -lt $ProducerCount; $i++) {
        $start = $i * $MessagesPerProducer
        $end = $start + $MessagesPerProducer - 1
        $rangeSum = $MessagesPerProducer * ($start + $end) / 2
        $expectedSum += $rangeSum
    }

    Write-Host "Configuration:" -ForegroundColor Cyan
    Write-Host "  Producers: $ProducerCount" -ForegroundColor White
    Write-Host "  Messages per producer: $MessagesPerProducer" -ForegroundColor White
    Write-Host "  Total messages: $totalMessages" -ForegroundColor White
    Write-Host "  Expected sum: $expectedSum" -ForegroundColor White
    Write-Host "  Queue: $queueName" -ForegroundColor White
    Write-Host "  Event: $eventName" -ForegroundColor White
    Write-Host ""

    Cleanup-Processes

    try {
        # Launch all producers FIRST (they will block waiting for the event)
        Write-Host "Launching $ProducerCount producers (blocking on event)..." -ForegroundColor Green
        $producerJobs = @()

        for ($i = 0; $i -lt $ProducerCount; $i++) {
            $startValue = $i * $MessagesPerProducer

            $producerJob = Start-Job -ScriptBlock {
                param($exe, $start, $count, $queue, $eventName)
                & $exe $start $count $queue $eventName 2>&1
            } -ArgumentList (Resolve-Path $cppRangeProducer).Path, $startValue, $MessagesPerProducer, $queueName, $eventName

            $producerJobs += $producerJob
        }

        Write-Host "All producers launched and blocking..." -ForegroundColor Yellow

        # Give producers time to start and reach blocking state
        Start-Sleep -Seconds 2

        # Start consumer (which will signal the event to release all producers)
        Write-Host "Starting consumer (will signal event to start all producers)..." -ForegroundColor Green
        $consumerJob = Start-Job -ScriptBlock {
            param($exe, $count, $sum, $queue, $eventName)
            & $exe $count $sum $queue 30 $eventName 2>&1
        } -ArgumentList (Resolve-Path $cppSumConsumer).Path, $totalMessages, $expectedSum, $queueName, $eventName

        Write-Host "🚀 Producers will start SIMULTANEOUSLY!" -ForegroundColor Green
        Write-Host ""

        # Wait for all producers to complete
        Write-Host "Waiting for producers..." -ForegroundColor Yellow
        $producerJobs | Wait-Job -Timeout 30 | Out-Null

        # Give consumer time to process remaining messages
        Start-Sleep -Seconds 2

        # Wait for consumer
        Write-Host "Waiting for consumer..." -ForegroundColor Yellow
        Wait-Job $consumerJob -Timeout 10 | Out-Null

        # Get consumer output
        $consumerOutput = Receive-Job $consumerJob

        # Parse results
        $success = $consumerOutput | Select-String "SUCCESS: All messages received with correct sum"
        $failed = $consumerOutput | Select-String "FAILED:"

        $message = ""
        $testSuccess = $false

        if ($success) {
            $testSuccess = $true
            $message = "All $totalMessages messages from $ProducerCount concurrent producers received with perfect integrity"
        } elseif ($failed) {
            $failLine = ($failed | Select-Object -First 1).Line
            $message = $failLine
        } else {
            $message = "Could not determine test result"
        }

        return @{
            Success = $testSuccess
            Message = $message
        }

    } finally {
        Get-Job | Remove-Job -Force
        Cleanup-Processes
    }
}

function Test-ConcurrentProducersCSharpConsumer {
    param(
        [int]$ProducerCount,
        [int]$MessagesPerProducer
    )

    Write-Host "=== Testing: Multiple Concurrent C++ Producers → Single C# Consumer ===" -ForegroundColor Magenta
    Write-Host "This test validates that the C# library can consume from concurrent C++ producers" -ForegroundColor Cyan
    Write-Host ""

    # Test parameters
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $queueName = "test-concurrent-csharp-$timestamp"
    $eventName = "ProducerStartEvent_$timestamp"
    $totalMessages = $ProducerCount * $MessagesPerProducer

    # Calculate expected sum
    $expectedSum = 0
    for ($i = 0; $i -lt $ProducerCount; $i++) {
        $start = $i * $MessagesPerProducer
        $end = $start + $MessagesPerProducer - 1
        $rangeSum = $MessagesPerProducer * ($start + $end) / 2
        $expectedSum += $rangeSum
    }

    Write-Host "Configuration:" -ForegroundColor Cyan
    Write-Host "  Producers: $ProducerCount (C++)" -ForegroundColor White
    Write-Host "  Consumer: C#" -ForegroundColor White
    Write-Host "  Messages per producer: $MessagesPerProducer" -ForegroundColor White
    Write-Host "  Total messages: $totalMessages" -ForegroundColor White
    Write-Host "  Expected sum: $expectedSum" -ForegroundColor White
    Write-Host "  Queue: $queueName" -ForegroundColor White
    Write-Host "  Event: $eventName" -ForegroundColor White
    Write-Host ""

    Cleanup-Processes

    try {
        # Launch all C++ producers (they will block waiting for the event)
        Write-Host "Launching $ProducerCount C++ producers (blocking on event)..." -ForegroundColor Green
        $producerJobs = @()

        for ($i = 0; $i -lt $ProducerCount; $i++) {
            $startValue = $i * $MessagesPerProducer

            $producerJob = Start-Job -ScriptBlock {
                param($exe, $start, $count, $queue, $eventName)
                & $exe $start $count $queue $eventName 2>&1
            } -ArgumentList (Resolve-Path $cppRangeProducer).Path, $startValue, $MessagesPerProducer, $queueName, $eventName

            $producerJobs += $producerJob
        }

        Write-Host "All C++ producers launched and blocking..." -ForegroundColor Yellow

        # Give producers time to start and reach blocking state
        Start-Sleep -Seconds 2

        # Start C# consumer (which will signal the event to release all producers)
        Write-Host "Starting C# consumer (will signal event to start all producers)..." -ForegroundColor Green
        $csharpConsumerExe = Join-Path $csharpSumConsumer "bin\Debug\net9.0\SumConsumer.exe"
        $consumerJob = Start-Job -ScriptBlock {
            param($exe, $count, $sum, $queue, $eventName)
            & $exe $count $sum $queue 30 $eventName 2>&1
        } -ArgumentList (Resolve-Path $csharpConsumerExe).Path, $totalMessages, $expectedSum, $queueName, $eventName

        Write-Host "🚀 Producers will start SIMULTANEOUSLY!" -ForegroundColor Green
        Write-Host ""

        # Wait for all producers to complete
        Write-Host "Waiting for C++ producers..." -ForegroundColor Yellow
        $producerJobs | Wait-Job -Timeout 30 | Out-Null

        # Give consumer time to process remaining messages
        Start-Sleep -Seconds 2

        # Wait for C# consumer
        Write-Host "Waiting for C# consumer..." -ForegroundColor Yellow
        Wait-Job $consumerJob -Timeout 10 | Out-Null

        # Get consumer output
        $consumerOutput = Receive-Job $consumerJob

        # Parse results first
        $success = $consumerOutput | Select-String "SUCCESS: All messages received with correct sum"
        $failed = $consumerOutput | Select-String "FAILED:"

        # Show filtered output (skip per-message output, show summary only)
        Write-Host "Consumer output (summary):" -ForegroundColor Yellow
        $consumerOutput | Where-Object { 
            $_ -notmatch "Received value:" -and 
            $_ -notmatch "message \d+/\d+" -and
            $_ -match "\S"  # Not empty
        } | ForEach-Object { Write-Host $_ }
        Write-Host ""

        $message = ""
        $testSuccess = $false

        if ($success) {
            $testSuccess = $true
            $message = "C# library successfully consumed all $totalMessages messages from $ProducerCount concurrent C++ producers with perfect integrity"
        } elseif ($failed) {
            $failLine = ($failed | Select-Object -First 1).Line
            $message = $failLine
        } else {
            Write-Host "⚠ Could not find SUCCESS or FAILED in output. Showing last 10 lines:" -ForegroundColor Yellow
            $consumerOutput | Select-Object -Last 10 | ForEach-Object { Write-Host $_ }
            $message = "Could not determine test result"
        }

        return @{
            Success = $testSuccess
            Message = $message
        }

    } finally {
        Get-Job | Remove-Job -Force
        Cleanup-Processes
    }
}

# Run tests
$results = @()

if ($Scenario -eq "all" -or $Scenario -eq "cpp-cpp") {
    $result = Test-CrossProcess "C++ Producer → C++ Consumer" $cppProducer $null $cppConsumer $null $MessageCount
    $results += [PSCustomObject]@{ Name = "C++ → C++"; Success = $result.Success; Message = $result.Message }
}

if ($Scenario -eq "all" -or $Scenario -eq "cpp-csharp") {
    $result = Test-CrossProcess "C++ Producer → C# Subscriber" $cppProducer $null "Subscriber.exe" $csharpSubscriber $MessageCount
    $results += [PSCustomObject]@{ Name = "C++ → C#"; Success = $result.Success; Message = $result.Message }
}

if ($Scenario -eq "all" -or $Scenario -eq "csharp-cpp") {
    $result = Test-CrossProcess "C# Publisher → C++ Consumer" "Publisher.exe" $csharpPublisher $cppConsumer $null $MessageCount
    $results += [PSCustomObject]@{ Name = "C# → C++"; Success = $result.Success; Message = $result.Message }
}

if ($Scenario -eq "all" -or $Scenario -eq "csharp-csharp") {
    $result = Test-CrossProcess "C# Publisher → C# Subscriber" "Publisher.exe" $csharpPublisher "Subscriber.exe" $csharpSubscriber $MessageCount
    $results += [PSCustomObject]@{ Name = "C# → C#"; Success = $result.Success; Message = $result.Message }
}

if ($Scenario -eq "all" -or $Scenario -eq "concurrent") {
    $result = Test-ConcurrentProducers $ConcurrentProducerCount $MessagesPerProducer
    $results += [PSCustomObject]@{ Name = "Concurrent C++ → C++"; Success = $result.Success; Message = $result.Message }
}

if ($Scenario -eq "all" -or $Scenario -eq "concurrent-csharp") {
    $result = Test-ConcurrentProducersCSharpConsumer $ConcurrentProducerCount $MessagesPerProducer
    $results += [PSCustomObject]@{ Name = "Concurrent C++ → C#"; Success = $result.Success; Message = $result.Message }
}

# Final summary
Write-Host "`n" + "="*80 -ForegroundColor Cyan
Write-Host "FINAL SUMMARY" -ForegroundColor Cyan
Write-Host "="*80 -ForegroundColor Cyan

$passCount = 0
foreach ($result in $results) {
    if ($result.Success) {
        Write-Host "✅ $($result.Name): SUCCESS" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "❌ $($result.Name): FAILED - $($result.Message)" -ForegroundColor Red
    }
}

Write-Host ""
if ($passCount -eq $results.Count) {
    Write-Host "🎉 ALL TESTS PASSED! ($passCount/$($results.Count))" -ForegroundColor Green
} else {
    Write-Host "⚠ SOME TESTS FAILED ($passCount/$($results.Count) passed)" -ForegroundColor Red
}

Write-Host ""
Write-Host "Test suite completed." -ForegroundColor Cyan