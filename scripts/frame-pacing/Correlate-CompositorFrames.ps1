[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $CaptureDirectory,

    [Parameter(Mandatory)]
    [string] $SurfaceFlingerCsv,

    [ValidateRange(1, 600)]
    [double] $ThresholdMs = 50,

    [ValidateRange(1, 200)]
    [double] $DisplayMissThresholdMs = 42.5,

    [string] $OutputCsv
)

$ErrorActionPreference = "Stop"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
[System.Threading.Thread]::CurrentThread.CurrentUICulture = [System.Globalization.CultureInfo]::InvariantCulture

$capturePath = (Resolve-Path -LiteralPath $CaptureDirectory).Path
$surfacePath = (Resolve-Path -LiteralPath $SurfaceFlingerCsv).Path
if (-not $OutputCsv) {
    $OutputCsv = Join-Path $capturePath "compositor-correlation.csv"
}

$window = Get-Content -Raw -LiteralPath (Join-Path $capturePath "measurement-window.json") |
    ConvertFrom-Json
$allFrames = @(Import-Csv -LiteralPath (Join-Path $capturePath "frame-timing.csv"))
if ($window.deferredTiming -eq $true) {
    $startNs = [uint64]$window.deviceMonotonicStartNs
    $endNs = [uint64]$window.deviceMonotonicEndNs
    $frames = @($allFrames | Where-Object {
        [uint64]$_.frame_start_ns -ge $startNs -and
        [uint64]$_.frame_start_ns -le $endNs
    })
} else {
    $frames = @($allFrames | Where-Object {
        [uint64]$_.frame_id -gt [uint64]$window.measurementStartAfterFrameId -and
        [uint64]$_.frame_id -le [uint64]$window.measurementEndAtFrameId
    })
}
if ($frames.Count -lt 2) {
    throw "The measurement window contains fewer than two native frame records."
}

$surfaceRows = @(
    Import-Csv -LiteralPath $surfacePath |
        Where-Object { [uint64]$_.actual_present_ns -gt 0 } |
        Sort-Object { [uint64]$_.actual_present_ns }
)
if ($surfaceRows.Count -lt 2) {
    throw "The SurfaceFlinger CSV contains fewer than two actual-present records."
}

function Get-CompositorWindow {
    param(
        [uint64] $WindowStartNs,
        [uint64] $WindowEndNs
    )

    # Include one display period on either side so a cadence gap crossing the
    # native frame interval is attributed to that interval.
    $paddingNs = [uint64] 100000000
    $nearRows = @($surfaceRows | Where-Object {
        [uint64]$_.actual_present_ns -ge ($WindowStartNs - $paddingNs) -and
        [uint64]$_.actual_present_ns -le ($WindowEndNs + $paddingNs)
    })
    $insideRows = @($nearRows | Where-Object {
        [uint64]$_.actual_present_ns -ge $WindowStartNs -and
        [uint64]$_.actual_present_ns -lt $WindowEndNs
    })

    [double]$maxGapMs = 0
    [int]$displayMissCount = 0
    if ($nearRows.Count -ge 2) {
        for ($index = 0; $index -lt ($nearRows.Count - 1); $index++) {
            $firstNs = [uint64]$nearRows[$index].actual_present_ns
            $secondNs = [uint64]$nearRows[$index + 1].actual_present_ns
            if ($secondNs -le $WindowStartNs -or $firstNs -ge $WindowEndNs) {
                continue
            }
            $gapMs = ($secondNs - $firstNs) / 1000000.0
            if ($gapMs -gt $maxGapMs) {
                $maxGapMs = $gapMs
            }
            if ($gapMs -ge $DisplayMissThresholdMs) {
                $displayMissCount++
            }
        }
    }

    $delays = @($insideRows | ForEach-Object {
        ([int64]$_.actual_present_ns - [int64]$_.desired_present_ns) / 1000000.0
    })
    $firstDeltaMs = $null
    $lastDeltaMs = $null
    if ($insideRows.Count -gt 0) {
        $firstDeltaMs = ([uint64]$insideRows[0].actual_present_ns - $WindowStartNs) / 1000000.0
        $lastDeltaMs = ([uint64]$insideRows[-1].actual_present_ns - $WindowStartNs) / 1000000.0
    }

    $maxDelayMs = $null
    $averageDelayMs = $null
    if ($delays.Count -gt 0) {
        $maxDelayMs = ($delays | Measure-Object -Maximum).Maximum
        $averageDelayMs = ($delays | Measure-Object -Average).Average
    }

    return [pscustomobject]@{
        actualPresentCount = $insideRows.Count
        firstPresentDeltaMs = $firstDeltaMs
        lastPresentDeltaMs = $lastDeltaMs
        maxActualGapMs = $maxGapMs
        displayMissCount = $displayMissCount
        maxPresentDelayMs = $maxDelayMs
        averagePresentDelayMs = $averageDelayMs
    }
}

$report = [System.Collections.Generic.List[object]]::new()
for ($index = 0; $index -lt ($frames.Count - 1); $index++) {
    $frame = $frames[$index]
    $nextFrame = $frames[$index + 1]
    $frameStartNs = [uint64]$frame.frame_start_ns
    $nextFrameStartNs = [uint64]$nextFrame.frame_start_ns
    $intervalMs = ($nextFrameStartNs - $frameStartNs) / 1000000.0
    if ($intervalMs -lt $ThresholdMs) {
        continue
    }

    $compositor = Get-CompositorWindow $frameStartNs $nextFrameStartNs
    $classification = if ($compositor.actualPresentCount -eq 0) {
        "no-present-in-interval"
    } elseif ($compositor.displayMissCount -gt 0) {
        "compositor-miss"
    } else {
        "native-only"
    }

    $report.Add([pscustomobject]@{
        frame_id = [uint64]$frame.frame_id
        interval_ms = [math]::Round($intervalMs, 3)
        frame_start_ns = $frameStartNs
        next_frame_start_ns = $nextFrameStartNs
        sf_actual_present_count = $compositor.actualPresentCount
        sf_first_present_delta_ms = if ($null -eq $compositor.firstPresentDeltaMs) { $null } else { [math]::Round($compositor.firstPresentDeltaMs, 3) }
        sf_last_present_delta_ms = if ($null -eq $compositor.lastPresentDeltaMs) { $null } else { [math]::Round($compositor.lastPresentDeltaMs, 3) }
        sf_max_actual_gap_ms = [math]::Round($compositor.maxActualGapMs, 3)
        sf_display_miss_count = $compositor.displayMissCount
        sf_max_present_delay_ms = if ($null -eq $compositor.maxPresentDelayMs) { $null } else { [math]::Round($compositor.maxPresentDelayMs, 3) }
        sf_average_present_delay_ms = if ($null -eq $compositor.averagePresentDelayMs) { $null } else { [math]::Round($compositor.averagePresentDelayMs, 3) }
        classification = $classification
    })
}

$report | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding utf8
Write-Host "Compositor-correlated outliers >= $ThresholdMs ms: $($report.Count)"
if ($report.Count -gt 0) {
    $report | Format-Table -AutoSize
    $report | Group-Object classification | Sort-Object Name | ForEach-Object {
        Write-Host ("{0}: {1}" -f $_.Name, $_.Count)
    }
}
Write-Host "Compositor correlation report: $OutputCsv"
