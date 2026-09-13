[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $CaptureDirectory,

    [ValidateRange(1, 600)]
    [double] $ThresholdMs = 50,

    [string] $OutputCsv
)

$ErrorActionPreference = "Stop"
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
[System.Threading.Thread]::CurrentThread.CurrentUICulture = [System.Globalization.CultureInfo]::InvariantCulture
$capturePath = (Resolve-Path -LiteralPath $CaptureDirectory).Path
if (-not $OutputCsv) {
    $OutputCsv = Join-Path $capturePath "outlier-correlation.csv"
}

$window = Get-Content -Raw -LiteralPath (Join-Path $capturePath "measurement-window.json") |
    ConvertFrom-Json
$allFrames = @(Import-Csv -LiteralPath (Join-Path $capturePath "frame-timing.csv"))
$workPath = Join-Path $capturePath "frame-work.csv"
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

$workRows = @()
if (Test-Path -LiteralPath $workPath -PathType Leaf) {
    if ($window.deferredTiming -eq $true) {
        $workRows = @(Import-Csv -LiteralPath $workPath | Where-Object {
            [uint64]$_.start_ns -ge $startNs -and
            [uint64]$_.start_ns -le $endNs
        })
    } else {
        $workRows = @(Import-Csv -LiteralPath $workPath | Where-Object {
            [uint64]$_.frame_id -gt [uint64]$window.measurementStartAfterFrameId -and
            [uint64]$_.frame_id -le [uint64]$window.measurementEndAtFrameId
        })
    }
}

$dxvkRows = @()
$dxvkPath = Join-Path $capturePath "dxvk-frame-timing.csv"
if (Test-Path -LiteralPath $dxvkPath -PathType Leaf) {
    $dxvkRows = @(Import-Csv -LiteralPath $dxvkPath)
}

$tileEvents = @(
    "scene_tilemap_predraw",
    "tilemap_predraw_calc",
    "tilemap_border_rebuild",
    "bump_tile_calc_total",
    "bump_tile_texture_lod0_total",
    "bump_tile_texture_lod1_total",
    "bump_tile_texture_lod2_total",
    "bump_tile_texture_lod3_total",
    "bump_tile_texture_lod4_total",
    "tilemap_draw_bump"
)
$queueSubmitEvents = @("queue_submit")
$queuePresentEvents = @("queue_present")
$acquireEvents = @("acquire_next_image")
$presentWaitEvents = @("wait_for_present_submission")
$latencyWaitEvents = @("frame_latency_wait")
$fpsLimiterEvents = @("fps_limiter")
$blockingEvents = @(
    "queue_present",
    "acquire_next_image",
    "wait_for_present_submission",
    "frame_latency_wait",
    "fps_limiter"
)

function Get-FrameWorkMilliseconds {
    param(
        [object[]] $Rows,
        [string[]] $Events
    )
    [double]$totalNs = 0
    [int]$count = 0
    foreach ($row in $Rows) {
        if ($Events -contains $row.event) {
            $startNs = [uint64]$row.start_ns
            $endNs = [uint64]$row.end_ns
            if ($endNs -ge $startNs) {
                $totalNs += $endNs - $startNs
                $count++
            }
        }
    }
    return [pscustomobject]@{
        milliseconds = $totalNs / 1000000.0
        count = $count
    }
}

function Get-FrameWorkCount {
    param(
        [object[]] $Rows,
        [string[]] $Events
    )
    [int64]$total = 0
    foreach ($row in $Rows) {
        if ($Events -contains $row.event) {
            if ($null -ne $row.value -and $row.value -ne "") {
                $total += [int64]$row.value
            } else {
                $total++
            }
        }
    }
    return $total
}

function Get-OverlappingMilliseconds {
    param(
        [object[]] $Rows,
        [string[]] $Events,
        [uint64] $WindowStartNs,
        [uint64] $WindowEndNs
    )
    [double]$totalNs = 0
    [int]$count = 0
    foreach ($row in $Rows) {
        if ($Events -contains $row.event) {
            $startNs = [uint64]$row.start_ns
            $endNs = [uint64]$row.end_ns
            if ($endNs -gt $WindowStartNs -and $startNs -lt $WindowEndNs) {
                $overlapStartNs = [math]::Max($startNs, $WindowStartNs)
                $overlapEndNs = [math]::Min($endNs, $WindowEndNs)
                if ($overlapEndNs -gt $overlapStartNs) {
                    $totalNs += $overlapEndNs - $overlapStartNs
                    $count++
                }
            }
        }
    }
    return [pscustomobject]@{
        milliseconds = $totalNs / 1000000.0
        count = $count
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

    $frameId = [uint64]$frame.frame_id
    $frameWork = @($workRows | Where-Object { [uint64]$_.frame_id -eq $frameId })
    $tileWork = Get-FrameWorkMilliseconds $frameWork $tileEvents
    $tilemapCalc = Get-FrameWorkMilliseconds $frameWork @("tilemap_predraw_calc")
    $borderRebuild = Get-FrameWorkMilliseconds $frameWork @("tilemap_border_rebuild")
    $bumpCalc = Get-FrameWorkMilliseconds $frameWork @("bump_tile_calc_total")
    $bumpCalcCount = Get-FrameWorkCount $frameWork @("bump_tile_calc_count")
    $lodCacheHitCount = Get-FrameWorkCount $frameWork @("tilemap_lod_cache_hit_count")
    $queueSubmit = Get-OverlappingMilliseconds $dxvkRows $queueSubmitEvents $frameStartNs $nextFrameStartNs
    $queuePresent = Get-OverlappingMilliseconds $dxvkRows $queuePresentEvents $frameStartNs $nextFrameStartNs
    $acquire = Get-OverlappingMilliseconds $dxvkRows $acquireEvents $frameStartNs $nextFrameStartNs
    $presentWait = Get-OverlappingMilliseconds $dxvkRows $presentWaitEvents $frameStartNs $nextFrameStartNs
    $latencyWait = Get-OverlappingMilliseconds $dxvkRows $latencyWaitEvents $frameStartNs $nextFrameStartNs
    $fpsLimiter = Get-OverlappingMilliseconds $dxvkRows $fpsLimiterEvents $frameStartNs $nextFrameStartNs
    $blocking = Get-OverlappingMilliseconds $dxvkRows $blockingEvents $frameStartNs $nextFrameStartNs

    $renderSubmitMs = ([uint64]$frame.render_submit_ns - $frameStartNs) / 1000000.0
    $presentCallMs = ([uint64]$frame.present_end_ns - [uint64]$frame.present_start_ns) / 1000000.0
    $frameSpanMs = ([uint64]$frame.present_end_ns - $frameStartNs) / 1000000.0
    $tilemapSignal = $tilemapCalc.milliseconds -ge 5 -or $borderRebuild.milliseconds -ge 5
    $presentationSignal = $presentCallMs -ge 5 -or $blocking.milliseconds -ge 5
    $classification = if ($tilemapSignal -and $presentationSignal) {
        "tilemap+presentation"
    } elseif ($tilemapSignal) {
        "tilemap"
    } elseif ($presentationSignal) {
        "presentation"
    } else {
        "other"
    }

    $report.Add([pscustomobject]@{
        frame_id = $frameId
        interval_ms = [math]::Round($intervalMs, 3)
        frame_span_ms = [math]::Round($frameSpanMs, 3)
        render_submit_ms = [math]::Round($renderSubmitMs, 3)
        present_call_ms = [math]::Round($presentCallMs, 3)
        tilemap_work_ms = [math]::Round($tileWork.milliseconds, 3)
        tilemap_event_count = $tileWork.count
        tilemap_predraw_calc_ms = [math]::Round($tilemapCalc.milliseconds, 3)
        tilemap_border_rebuild_ms = [math]::Round($borderRebuild.milliseconds, 3)
        bump_tile_calc_ms = [math]::Round($bumpCalc.milliseconds, 3)
        bump_tile_calc_count = $bumpCalcCount
        tilemap_lod_cache_hit_count = $lodCacheHitCount
        dxvk_queue_submit_overlap_ms = [math]::Round($queueSubmit.milliseconds, 3)
        dxvk_queue_present_overlap_ms = [math]::Round($queuePresent.milliseconds, 3)
        dxvk_acquire_overlap_ms = [math]::Round($acquire.milliseconds, 3)
        dxvk_present_wait_overlap_ms = [math]::Round($presentWait.milliseconds, 3)
        dxvk_frame_latency_wait_overlap_ms = [math]::Round($latencyWait.milliseconds, 3)
        dxvk_fps_limiter_overlap_ms = [math]::Round($fpsLimiter.milliseconds, 3)
        dxvk_blocking_overlap_ms = [math]::Round($blocking.milliseconds, 3)
        dxvk_blocking_event_count = $blocking.count
        classification = $classification
    })
}

$report | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding utf8
Write-Host "Outliers >= $ThresholdMs ms: $($report.Count)"
if ($report.Count -gt 0) {
    $report | Format-Table -AutoSize
}
Write-Host "Correlation report: $OutputCsv"
