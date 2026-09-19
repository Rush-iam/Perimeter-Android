[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet("sokol", "dxvk1")]
    [string] $Renderer,

    [Parameter(Mandatory)]
    [ValidateRange(1, 99)]
    [int] $Run,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string] $Scenario,

    [ValidateSet("debug", "release", "releaseBenchmark")]
    [string] $BuildType = "debug",

    [ValidateSet("O2", "O3")]
    [string] $Optimization = "O2",

    [ValidateSet("OFF", "ON")]
    [string] $ThinLto = "OFF",

    [ValidateSet(50, 75, 100)]
    [int] $ResolutionScale = 75,

    [ValidateSet("full-refresh", "half-refresh")]
    [string] $FrameRate = "half-refresh",

    [ValidateRange(0, 3600)]
    [int] $WarmupSeconds = 10,

    [ValidateRange(0, 3600)]
    [int] $HighCpuLoadSeconds = 3,

    [ValidateRange(100, 10000)]
    [int] $PostLoadFlushWaitMilliseconds = 2000,

    [ValidateRange(1, 3600)]
    [int] $CaptureSeconds = 60,

    [ValidateSet("none", "held-square-5x", "held-square-15x", "zoom-out-in-7x", "zoom-out-in-21x", "zoom-out-in-1x")]
    [string] $CameraPan = "held-square-5x",

    [switch] $AssumeReady,

    [string] $Serial,
    [string] $Adb,
    [string] $OutputRoot = "captures/frame-pacing"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA "Android/Sdk/platform-tools/adb.exe"
}
if (-not (Test-Path -LiteralPath $Adb -PathType Leaf)) {
    throw "adb was not found at '$Adb'. Pass -Adb with the Android platform-tools path."
}

$adbPrefix = @()
if ($Serial) {
    $adbPrefix = @("-s", $Serial)
}
function Invoke-Adb {
    param([Parameter(ValueFromRemainingArguments)] [string[]] $Arguments)
    $result = & $Adb @adbPrefix @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed ($LASTEXITCODE): $($Arguments -join ' ')"
    }
    return $result
}

$devices = @(
    @(Invoke-Adb devices) | ForEach-Object {
        if ($_ -match '^([^\s]+)\s+device(?:\s|$)') { $Matches[1] }
    }
)
if (-not $Serial) {
    if ($devices.Count -ne 1) {
        throw "Expected exactly one connected device, found $($devices.Count). Pass -Serial when multiple devices are attached."
    }
    $Serial = $devices[0]
    $adbPrefix = @("-s", $Serial)
}

$safeScenario = $Scenario -replace "[^A-Za-z0-9._-]", "_"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runName = "{0}-{1}-{2}-run{3:D2}-{4}" -f $safeScenario, $Renderer, $FrameRate, $Run, $timestamp
$outputDirectory = Join-Path (Join-Path $projectRoot $OutputRoot) $runName
New-Item -ItemType Directory -Path $outputDirectory | Out-Null

function Save-AdbOutput {
    param([string] $Name, [string[]] $Arguments)
    @(Invoke-Adb @Arguments) | Set-Content -LiteralPath (Join-Path $outputDirectory $Name) -Encoding utf8
}

function Write-NativeControl {
    param([Parameter(Mandatory)] [string] $Value)
    # Avoid nested sh -c redirection here. On Windows adb, that quoting can
    # reach the device shell without the intended working directory. Feeding
    # the marker to toybox tee keeps the run-as path and file creation stable.
    $Value | & $Adb @adbPrefix shell run-as com.queststoredb.perimeter toybox tee files/camera-motion-control | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed while writing the native timing control marker."
    }
}

function Flush-NativeTiming {
    param([int] $WaitMilliseconds = 1000)
    # Native timing uses a large userspace buffer to keep CSV writes off the
    # render-critical path. Flush before and after the measurement so a valid
    # tail from the previous run cannot be mistaken for a live boundary.
    Write-NativeControl "flush-frame-timing-v1"
    Start-Sleep -Milliseconds $WaitMilliseconds
}

function Invoke-HighCpuLoad {
    if ($HighCpuLoadSeconds -le 0) {
        return
    }

    Write-Host "High CPU load: $HighCpuLoadSeconds seconds across all available device CPUs"
    # Use one busy toybox process per available CPU and clean them up before
    # the command returns. Keep the command free of nested quotes because adb
    # passes the remote shell expression through another argument parser.
    $loadCommand = 'pids=""; for i in $(seq 1 $(nproc)); do yes >/dev/null & pids="$pids $!"; done; sleep ' +
        $HighCpuLoadSeconds + '; for pid in $pids; do kill "$pid" 2>/dev/null || true; done'
    Invoke-Adb shell "sh -c '$loadCommand'" | Out-Null
}

function Get-NativeTimingTail {
    return ((Invoke-Adb "exec-out" "run-as" "com.queststoredb.perimeter" "tail" "-n" "1" "files/frame-timing.csv") -join "").Trim()
}

$revision = (& git -c "safe.directory=$($projectRoot -replace '\\', '/')" -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Unable to read the Git revision." }
$dirty = [bool](& git -c "safe.directory=$($projectRoot -replace '\\', '/')" -C $projectRoot status --porcelain)

$metadata = [ordered]@{
    schemaVersion = 1
    capturedAt = (Get-Date).ToUniversalTime().ToString("o")
    revision = $revision
    workingTreeDirty = $dirty
    buildType = $BuildType
    optimizationLevel = $Optimization
    thinLto = $ThinLto
    deviceSerial = $Serial
    manufacturer = ((Invoke-Adb shell getprop ro.product.manufacturer) -join "").Trim()
    model = ((Invoke-Adb shell getprop ro.product.model) -join "").Trim()
    androidVersion = ((Invoke-Adb shell getprop ro.build.version.release) -join "").Trim()
    androidSdk = ((Invoke-Adb shell getprop ro.build.version.sdk) -join "").Trim()
    emuiVersion = ((Invoke-Adb shell getprop ro.build.version.emui) -join "").Trim()
    displayBuild = ((Invoke-Adb shell getprop ro.build.display.id) -join "").Trim()
    buildFingerprint = ((Invoke-Adb shell getprop ro.build.fingerprint) -join "").Trim()
    renderer = $Renderer
    resolutionScalePercent = $ResolutionScale
    frameRateSetting = $FrameRate
    scenario = $Scenario
    warmupSeconds = $WarmupSeconds
    highCpuLoadSeconds = $HighCpuLoadSeconds
    postLoadFlushWaitMilliseconds = $PostLoadFlushWaitMilliseconds
    captureSeconds = $CaptureSeconds
    cameraPan = $CameraPan
    run = $Run
}
$metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputDirectory "metadata.json") -Encoding utf8

Save-AdbOutput "display.txt" @("shell", "dumpsys", "display")
Save-AdbOutput "surfaceflinger.txt" @("shell", "dumpsys", "SurfaceFlinger")
Save-AdbOutput "package.txt" @("shell", "dumpsys", "package", "com.queststoredb.perimeter")
Save-AdbOutput "battery-before.txt" @("shell", "dumpsys", "battery")
Save-AdbOutput "thermal-before.txt" @("shell", "dumpsys", "thermalservice")
Save-AdbOutput "huawei-power-settings.txt" @("shell", "settings", "get", "system", "SmartModeStatus")

Write-Host "Run $Run ready: $Renderer / $FrameRate / $ResolutionScale%"
if ($AssumeReady) {
    Write-Host "Using the current verified gameplay scene."
} else {
    Write-Host "Put the benchmark at its start marker, then press Enter."
    [void](Read-Host)
}

Invoke-Adb logcat -c | Out-Null
Invoke-Adb shell dumpsys gfxinfo com.queststoredb.perimeter reset | Out-Null
Invoke-Adb shell run-as com.queststoredb.perimeter rm -f files/camera-motion-control | Out-Null
if ($WarmupSeconds -gt 0) {
    Write-Host "Warm-up: $WarmupSeconds seconds"
    Start-Sleep -Seconds $WarmupSeconds
}

Invoke-HighCpuLoad

Flush-NativeTiming -WaitMilliseconds $PostLoadFlushWaitMilliseconds
$startTimingLine = Get-NativeTimingTail
$startTimingFields = $startTimingLine.Split(',')
if ($startTimingFields.Count -ne 5 -or $startTimingFields[0] -notmatch '^\d+$') {
    $headerLine = ((Invoke-Adb "exec-out" "run-as" "com.queststoredb.perimeter" "head" "-n" "1" "files/frame-timing.csv") -join "").Trim()
    if ($headerLine -eq "frame_id,frame_start_ns,render_submit_ns,present_start_ns,present_end_ns") {
        throw "Native frame timing has no row after the warm-up flush. Keep the game on the HUD and retry."
    } else {
        throw "Native frame timing is unavailable. Enable Diagnostics > Record frame timing and relaunch."
    }
}
$deferredTiming = $false
$measurementStartAfterFrameId = [uint64]$startTimingFields[0]
Write-Host "Capturing: $CaptureSeconds seconds from native frame $measurementStartAfterFrameId"
$deviceStartSeconds = [double](((Invoke-Adb shell cat /proc/uptime) -join " ").Split(' ')[0])
$captureStart = (Get-Date).ToUniversalTime().ToString("o")
if ($CameraPan -eq "held-square-5x") {
    if ($CaptureSeconds -ne 20) {
        throw "-CaptureSeconds must be 20 when -CameraPan held-square-5x is used."
    }
    Write-Host "Camera motion: hold Right, Up, Left, Down for 1 second each; repeat 5 times"
    # Keep the redirection inside run-as. Passing -c and its command as separate
    # adb arguments loses the quoting before it reaches the device shell.
    Write-NativeControl "held-square-5x-v1"
    Start-Sleep -Seconds $CaptureSeconds
} elseif ($CameraPan -eq "held-square-15x") {
    if ($CaptureSeconds -ne 60) {
        throw "-CaptureSeconds must be 60 when -CameraPan held-square-15x is used."
    }
    Write-Host "Camera motion: hold Right, Up, Left, Down for 1 second each; repeat 15 times"
    Write-NativeControl "held-square-15x-v1"
    Start-Sleep -Seconds $CaptureSeconds
} elseif ($CameraPan -eq "zoom-out-in-7x") {
    if ($CaptureSeconds -ne 21) {
        throw "-CaptureSeconds must be 21 when -CameraPan zoom-out-in-7x is used."
    }
    Write-Host "Camera motion: zoom out for 1.5 seconds, zoom in for 1.5 seconds; repeat 7 times"
    Write-NativeControl "zoom-out-in-7x-v1"
    Start-Sleep -Seconds $CaptureSeconds
} elseif ($CameraPan -eq "zoom-out-in-21x") {
    if ($CaptureSeconds -ne 63) {
        throw "-CaptureSeconds must be 63 when -CameraPan zoom-out-in-21x is used."
    }
    Write-Host "Camera motion: zoom out for 1.5 seconds, zoom in for 1.5 seconds; repeat 21 times"
    Write-NativeControl "zoom-out-in-21x-v1"
    Start-Sleep -Seconds $CaptureSeconds
} elseif ($CameraPan -eq "zoom-out-in-1x") {
    if ($CaptureSeconds -ne 3) {
        throw "-CaptureSeconds must be 3 when -CameraPan zoom-out-in-1x is used."
    }
    Write-Host "Camera motion: zoom out for 1.5 seconds, zoom in for 1.5 seconds; one cycle"
    Write-NativeControl "zoom-out-in-1x-v1"
    Start-Sleep -Seconds $CaptureSeconds
} else {
    Start-Sleep -Seconds $CaptureSeconds
}
$captureEnd = (Get-Date).ToUniversalTime().ToString("o")
$deviceEndSeconds = [double](((Invoke-Adb shell cat /proc/uptime) -join " ").Split(' ')[0])
Flush-NativeTiming
$endTimingLine = Get-NativeTimingTail
$endTimingFields = $endTimingLine.Split(',')
if ($endTimingFields.Count -ne 5 -or $endTimingFields[0] -notmatch '^\d+$') {
    throw "Native frame timing did not flush after capture."
}
$measurementEndAtFrameId = [uint64]$endTimingLine.Split(',')[0]
if ($measurementEndAtFrameId -le $measurementStartAfterFrameId) {
    throw "Native frame timing did not advance during capture (start frame $measurementStartAfterFrameId, end frame $measurementEndAtFrameId)."
}

Save-AdbOutput "logcat.txt" @("logcat", "-d", "-v", "threadtime")
Save-AdbOutput "gfxinfo-framestats.txt" @("shell", "dumpsys", "gfxinfo", "com.queststoredb.perimeter", "framestats")
Save-AdbOutput "display-after.txt" @("shell", "dumpsys", "display")
Save-AdbOutput "battery-after.txt" @("shell", "dumpsys", "battery")
Save-AdbOutput "thermal-after.txt" @("shell", "dumpsys", "thermalservice")
Save-AdbOutput "frame-timing.csv" @("exec-out", "run-as", "com.queststoredb.perimeter",
    "cat", "files/frame-timing.csv")
Save-AdbOutput "frame-work.csv" @("exec-out", "run-as", "com.queststoredb.perimeter",
    "cat", "files/frame-work.csv")
if ($Renderer -eq "dxvk1") {
    & $Adb @adbPrefix exec-out run-as com.queststoredb.perimeter cat files/dxvk-frame-timing.csv |
        Set-Content -LiteralPath (Join-Path $outputDirectory "dxvk-frame-timing.csv") -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw "DXVK internal frame timing is unavailable." }
}

$window = [ordered]@{
    measurementStart = $captureStart
    measurementEnd = $captureEnd
    deviceMonotonicStartNs = [math]::Round($deviceStartSeconds * 1000000000)
    deviceMonotonicEndNs = [math]::Round($deviceEndSeconds * 1000000000)
    measurementStartAfterFrameId = $measurementStartAfterFrameId
    measurementEndAtFrameId = $measurementEndAtFrameId
    deferredTiming = $deferredTiming
}
$window | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputDirectory "measurement-window.json") -Encoding utf8
Write-Host "Raw capture saved to $outputDirectory"
