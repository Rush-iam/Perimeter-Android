[CmdletBinding()]
param(
    [ValidateRange(1, 3600)] [int] $CaptureSeconds = 60,
    [ValidateRange(1, 10)] [int] $PollSeconds = 3,
    [switch] $Passive,
    [string] $Serial,
    [string] $Adb,
    [string] $OutputPath
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
if (-not $Adb) { $Adb = Join-Path $env:LOCALAPPDATA "Android/Sdk/platform-tools/adb.exe" }
$adbPrefix = if ($Serial) { @("-s", $Serial) } else { @() }

function Invoke-Adb([Parameter(ValueFromRemainingArguments)] [string[]] $Arguments) {
    $result = & $Adb @adbPrefix @Arguments
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' ')" }
    return $result
}
function Invoke-Remote([string] $Command) {
    $result = & $Adb @adbPrefix shell $Command
    if ($LASTEXITCODE -ne 0) { throw "adb shell failed: $Command" }
    return $result
}

$layer = @(Invoke-Adb shell dumpsys SurfaceFlinger --list) |
    Where-Object { $_ -match '^SurfaceView - com\.queststoredb\.perimeter/.+MainActivity#\d+$' } |
    Select-Object -Last 1
if (-not $layer) { throw "The active Perimeter SurfaceView layer was not found." }
$layer = $layer.Trim()

if (-not $OutputPath) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputPath = Join-Path $projectRoot "captures/frame-pacing/surfaceflinger-single-present-$timestamp.csv"
}
$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $parent -Force | Out-Null

$rows = [System.Collections.Generic.Dictionary[uint64,string]]::new()
function Drain-Latency {
    $output = @(Invoke-Remote "dumpsys SurfaceFlinger --latency '$layer'")
    foreach ($line in $output | Select-Object -Skip 1) {
        if ($line -match '^(\d+)\s+(\d+)\s+(\d+)$') {
            $actual = [uint64]$Matches[2]
            if ($actual -ne 9223372036854775807) { $rows[$actual] = $line -replace '\s+', ',' }
        }
    }
}

Invoke-Remote "dumpsys SurfaceFlinger --latency-clear '$layer'" | Out-Null
$directions = @("KEYCODE_DPAD_RIGHT", "KEYCODE_DPAD_DOWN", "KEYCODE_DPAD_LEFT", "KEYCODE_DPAD_UP")
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$nextPoll = [double]$PollSeconds
while ($timer.Elapsed.TotalSeconds -lt $CaptureSeconds) {
    if (-not $Passive) {
        $leg = [math]::Min(3, [math]::Floor(4 * $timer.Elapsed.TotalSeconds / $CaptureSeconds))
        Invoke-Adb shell input keyevent --longpress $directions[$leg] | Out-Null
    }
    if ($timer.Elapsed.TotalSeconds -ge $nextPoll) {
        Drain-Latency
        $nextPoll += $PollSeconds
    }
}
Drain-Latency

"desired_present_ns,actual_present_ns,frame_ready_ns" | Set-Content -LiteralPath $OutputPath -Encoding ascii
$rows.Keys | Sort-Object | ForEach-Object { $rows[$_] } |
    Add-Content -LiteralPath $OutputPath -Encoding ascii
Write-Host "SurfaceFlinger cadence saved to $OutputPath ($($rows.Count) frames)."
