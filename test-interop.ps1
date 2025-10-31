# Test script for C++ Producer -> C# Subscriber communication
# This script starts both processes and waits for them to complete

Write-Host "=== C++ Producer -> C# Subscriber Interoperability Test ===" -ForegroundColor Cyan
Write-Host ""

# Paths
$subscriberPath = "src\Sample\csharp\Subscriber"
$producerPath = "C:\repos\interprocess\src\x64\Debug\Producer.exe"

# Kill any existing processes
Write-Host "Cleaning up any existing processes..." -ForegroundColor Yellow
taskkill /F /IM Producer.exe 2>$null | Out-Null
taskkill /F /IM Subscriber.exe 2>$null | Out-Null
Start-Sleep -Seconds 1

try {
    # Start C++ Producer first (it will create the queue)
    Write-Host "Starting C++ Producer first (will run for 30 seconds)..." -ForegroundColor Green
    $producerJob = Start-Job -ScriptBlock {
        param($path)
        & $path 2>&1
    } -ArgumentList (Resolve-Path $producerPath).Path
    
    # Wait a moment for producer to initialize and create the queue
    Start-Sleep -Seconds 3
    
    # Start C# Subscriber (it will connect to the existing queue)
    Write-Host "Starting C# Subscriber to read from the existing queue..." -ForegroundColor Green
    $subscriberJob = Start-Job -ScriptBlock {
        param($path)
        Set-Location $path
        dotnet run --no-build 2>&1
    } -ArgumentList (Resolve-Path $subscriberPath).Path
    
    # Check if subscriber started successfully
    Start-Sleep -Seconds 2
    $subscriberStatus = Receive-Job $subscriberJob -Keep
    if ($subscriberStatus -match "exception|error") {
        Write-Host "Subscriber failed to start properly:" -ForegroundColor Red
        Write-Host $subscriberStatus -ForegroundColor Red
        throw "Subscriber startup failed"
    }
    
    Write-Host "Both processes are running..." -ForegroundColor Cyan
    Write-Host "Waiting for C++ Producer to complete (30 seconds)..." -ForegroundColor Cyan
    
    # Wait for producer to complete (should take ~30 seconds)
    $startTime = Get-Date
    Write-Host "Monitoring both processes..." -ForegroundColor Cyan
    
    # Monitor both processes
    while ((Get-Date) - $startTime -lt [TimeSpan]::FromSeconds(35)) {
        # Show any new subscriber output
        $newSubscriberOutput = Receive-Job $subscriberJob -Keep
        if ($newSubscriberOutput) {
            $newSubscriberOutput | Where-Object { $_ -ne $null -and $_ -ne "" } | ForEach-Object { 
                Write-Host "[SUBSCRIBER] $_" -ForegroundColor Green 
            }
        }
        
        # Check if producer is done
        if ($producerJob.State -eq "Completed") {
            $elapsedSeconds = ([int]((Get-Date) - $startTime).TotalSeconds)
            Write-Host "Producer completed after $elapsedSeconds seconds" -ForegroundColor Cyan
            break
        }
        
        Start-Sleep -Seconds 1
    }
    
    # Get all output
    $producerOutput = Receive-Job $producerJob
    
    # Get final subscriber output
    $subscriberOutput = Receive-Job $subscriberJob
    
    # Stop subscriber job
    Stop-Job $subscriberJob -PassThru | Remove-Job
    
    Write-Host ""
    Write-Host "=== RESULTS ===" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "C++ Producer Output:" -ForegroundColor Yellow
    Write-Host "-------------------" -ForegroundColor Yellow
    $producerOutput | ForEach-Object { Write-Host $_ }
    
    Write-Host ""
    Write-Host "C# Subscriber Output:" -ForegroundColor Yellow
    Write-Host "--------------------" -ForegroundColor Yellow
    $subscriberOutput | ForEach-Object { Write-Host $_ }
    
    # Analyze results
    Write-Host ""
    Write-Host "=== ANALYSIS ===" -ForegroundColor Cyan
    
    $producerMessages = ($producerOutput | Select-String "Total messages sent:" | ForEach-Object { $_.Line -replace ".*Total messages sent: (\d+).*", '$1' })
    $subscriberMessages = ($subscriberOutput | Select-String "Dequeue #" | Measure-Object).Count
    
    if ($producerMessages) {
        Write-Host "Producer sent: $producerMessages messages" -ForegroundColor Green
    } else {
        Write-Host "Could not determine producer message count" -ForegroundColor Red
    }
    
    if ($subscriberMessages -gt 0) {
        Write-Host "Subscriber received: $subscriberMessages messages" -ForegroundColor Green
        Write-Host ""
        if ($producerMessages -and $subscriberMessages -gt 0) {
            $percentage = [math]::Round(($subscriberMessages / [int]$producerMessages) * 100, 2)
            Write-Host "SUCCESS: Communication working! Subscriber received $percentage% of sent messages" -ForegroundColor Green
        } else {
            Write-Host "SUCCESS: Subscriber received messages from C++ Producer!" -ForegroundColor Green
        }
    } else {
        Write-Host "ISSUE: Subscriber received 0 messages" -ForegroundColor Red
        Write-Host "This suggests the interprocess communication is not working properly" -ForegroundColor Red
    }
    
} catch {
    Write-Host "Test failed with error: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    # Cleanup
    Write-Host ""
    Write-Host "Cleaning up..." -ForegroundColor Yellow
    Get-Job | Remove-Job -Force
    taskkill /F /IM Producer.exe 2>$null | Out-Null
    taskkill /F /IM Subscriber.exe 2>$null | Out-Null
}

Write-Host ""
Write-Host "Test completed." -ForegroundColor Cyan