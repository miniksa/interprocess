# Comprehensive Cross-Process Message Integrity Test
# Tests all four scenarios: C++ -> C#, C# -> C++, C++ -> C++, C# -> C#
# Validates sequential values 0-99 and exact message counts

param(
    [int]$MessageCount = 100,
    [string]$Scenario = "all"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Cross-Process Message Integrity Test Suite ===" -ForegroundColor Cyan
Write-Host "Testing with $MessageCount messages (values 0-99 sequentially)" -ForegroundColor Cyan
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
$csharpPublisher = "src\Sample\csharp\Publisher"
$csharpSubscriber = "src\Sample\csharp\Subscriber"

# Verify paths exist
$paths = @{
    "C++ Producer" = $cppProducer
    "C++ Consumer" = $cppConsumer
    "C# Publisher" = "$csharpPublisher\bin\Debug\net9.0\Publisher.exe"
    "C# Subscriber" = "$csharpSubscriber\bin\Debug\net9.0\Subscriber.exe"
}

foreach ($name in $paths.Keys) {
    if (!(Test-Path $paths[$name])) {
        throw "$name not found at $($paths[$name])"
    }
}

Write-Host "All executables found!" -ForegroundColor Green
Write-Host ""

function Cleanup-Processes {
    taskkill /F /IM Producer.exe 2>$null | Out-Null
    taskkill /F /IM Consumer.exe 2>$null | Out-Null
    taskkill /F /IM Publisher.exe 2>$null | Out-Null
    taskkill /F /IM Subscriber.exe 2>$null | Out-Null
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