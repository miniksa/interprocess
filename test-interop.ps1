# Test script for C++ Producer -> C# Subscriber communication with exact message counting
# cd c:\repos\interprocess\src && msbuild /t:rebuild && C:\repos\interprocess\src\x64\Debug\Interprocess.Native.Static.Tests.exe && cd "c:\repos\interprocess" && .\test-interop.ps1
param(
    [int]$MessageCount = 500
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== Cross-Process Message Integrity Test ===" -ForegroundColor Cyan
Write-Host "Testing with $MessageCount messages" -ForegroundColor Cyan
Write-Host ""

# Paths
$subscriberPath = "$scriptDir\src\Sample\csharp\Subscriber"
$producerPath = "$scriptDir\src\x64\Debug\Producer.exe"

# Kill any existing processes
Write-Host "Cleaning up any existing processes..." -ForegroundColor Yellow
taskkill /F /IM Producer.exe 2>$null | Out-Null
taskkill /F /IM Subscriber.exe 2>$null | Out-Null
Start-Sleep -Seconds 1

# now run msbuild from the src dir
Write-Host "Building the project..." -ForegroundColor Yellow
Push-Location "$scriptDir\src"
try {
    msbuild /t:rebuild
} finally {
    Pop-Location
}
#ensure the test units exist
if(-not Test-Path $subscriberPath) {
    Write-Host "Subscriber project not found at $subscriberPath" -ForegroundColor Red
    throw "Subscriber project not found"
}

if(-not Test-Path $producerPath) {
    Write-Host "Producer project not found at $producerPath" -ForegroundColor Red
    throw "Producer project not found"
}

Write-Host "Testing interprocess communication..." -ForegroundColor Yellow

try {
    # Start C# Subscriber first (will create or connect to queue)
    Write-Host "Starting C# Subscriber first..." -ForegroundColor Green
    $subscriberJob = Start-Job -ScriptBlock {
        param($path)
        Set-Location $path
        dotnet run --no-build 2>&1
    } -ArgumentList (Resolve-Path $subscriberPath).Path
    
    # Wait for subscriber to initialize
    Start-Sleep -Seconds 3
    
    # Check if subscriber started successfully
    $subscriberStatus = Receive-Job $subscriberJob -Keep
    if ($subscriberStatus -match "exception|error") {
        Write-Host "Subscriber failed to start properly:" -ForegroundColor Red
        Write-Host $subscriberStatus -ForegroundColor Red
        throw "Subscriber startup failed"
    }
    
    # Start C++ Producer with specific message count
    Write-Host "Starting C++ Producer to send $MessageCount messages..." -ForegroundColor Green
    $producerJob = Start-Job -ScriptBlock {
        param($path, $count)
        & $path $count 2>&1
    } -ArgumentList (Resolve-Path $producerPath).Path, $MessageCount
    
    Write-Host "Both processes are running..." -ForegroundColor Cyan
    Write-Host "Waiting for C++ Producer to complete..." -ForegroundColor Cyan
    
    # Wait for producer to complete
    $startTime = Get-Date
    Write-Host "Monitoring both processes..." -ForegroundColor Cyan
    
    # Monitor both processes
    while ((Get-Date) - $startTime -lt [TimeSpan]::FromSeconds(60)) {
        # Check if producer is done
        if ($producerJob.State -eq "Completed") {
            $elapsedSeconds = ([int]((Get-Date) - $startTime).TotalSeconds)
            Write-Host "Producer completed after $elapsedSeconds seconds" -ForegroundColor Cyan
            break
        }
        
        Start-Sleep -Seconds 1
    }
    
    # Wait a bit more for subscriber to finish processing
    Write-Host "Waiting for subscriber to finish processing..." -ForegroundColor Cyan
    Start-Sleep -Seconds 5
    
    # Get all output
    $producerOutput = Receive-Job $producerJob
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
    
    # Extract message counts
    $producerSentPattern = "Total messages sent: (\d+)"
    $subscriberReceivedPattern = "Total messages received: (\d+)"
    
    $producerMatch = $producerOutput | Select-String $producerSentPattern
    $subscriberMatch = $subscriberOutput | Select-String $subscriberReceivedPattern
    
    $producerSent = 0
    $subscriberReceived = 0
    
    if ($producerMatch) {
        $producerSent = [int]$producerMatch.Matches[0].Groups[1].Value
        Write-Host "✓ Producer sent: $producerSent messages" -ForegroundColor Green
    } else {
        Write-Host "✗ Could not determine producer message count" -ForegroundColor Red
        Write-Host "Producer output: $($producerOutput -join '; ')" -ForegroundColor Yellow
    }
    
    if ($subscriberMatch) {
        $subscriberReceived = [int]$subscriberMatch.Matches[0].Groups[1].Value
        Write-Host "✓ Subscriber received: $subscriberReceived messages" -ForegroundColor Green
    } else {
        Write-Host "✗ Could not determine subscriber message count" -ForegroundColor Red
        Write-Host "Subscriber output: $($subscriberOutput -join '; ')" -ForegroundColor Yellow
    }
    
    Write-Host ""
    
    # Verify exact match
    if ($producerSent -eq $MessageCount -and $subscriberReceived -eq $MessageCount) {
        Write-Host "🎉 SUCCESS: Perfect message integrity!" -ForegroundColor Green
        Write-Host "   Expected: $MessageCount messages" -ForegroundColor Green
        Write-Host "   Sent: $producerSent messages" -ForegroundColor Green
        Write-Host "   Received: $subscriberReceived messages" -ForegroundColor Green
        Write-Host "   Loss rate: 0%" -ForegroundColor Green
    } elseif ($producerSent -eq $subscriberReceived) {
        Write-Host "✓ SUCCESS: All sent messages were received!" -ForegroundColor Green
        Write-Host "   Sent: $producerSent messages" -ForegroundColor Green
        Write-Host "   Received: $subscriberReceived messages" -ForegroundColor Green
        if ($producerSent -ne $MessageCount) {
            Write-Host "⚠ WARNING: Producer sent $producerSent instead of expected $MessageCount" -ForegroundColor Yellow
        }
    } else {
        Write-Host "❌ FAILURE: Message count mismatch!" -ForegroundColor Red
        Write-Host "   Expected: $MessageCount messages" -ForegroundColor Red
        Write-Host "   Sent: $producerSent messages" -ForegroundColor Red
        Write-Host "   Received: $subscriberReceived messages" -ForegroundColor Red
        if ($producerSent -gt 0) {
            $lossRate = [math]::Round((($producerSent - $subscriberReceived) / $producerSent) * 100, 2)
            Write-Host "   Loss rate: $lossRate%" -ForegroundColor Red
        }
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