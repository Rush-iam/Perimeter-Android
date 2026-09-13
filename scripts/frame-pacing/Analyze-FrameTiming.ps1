[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $CaptureDirectory
)

$ErrorActionPreference = "Stop"
$capture = (Resolve-Path -LiteralPath $CaptureDirectory).Path
$window = Get-Content -Raw -LiteralPath (Join-Path $capture "measurement-window.json") |
    ConvertFrom-Json
$allRows = @(Import-Csv -LiteralPath (Join-Path $capture "frame-timing.csv"))
$workPath = Join-Path $capture "frame-work.csv"
if ($window.deferredTiming -eq $true) {
    $startNs = [uint64]$window.deviceMonotonicStartNs
    $endNs = [uint64]$window.deviceMonotonicEndNs
    $rows = @($allRows | Where-Object {
        [uint64]$_.frame_start_ns -ge $startNs -and
        [uint64]$_.frame_start_ns -le $endNs
    })
} elseif ($null -ne $window.measurementStartAfterFrameId) {
    $rows = @($allRows | Where-Object {
        [uint64]$_.frame_id -gt [uint64]$window.measurementStartAfterFrameId -and
        [uint64]$_.frame_id -le [uint64]$window.measurementEndAtFrameId
    })
} else {
    # Compatibility for the first instrumented capture, before frame-ID
    # boundary markers were added. Anchor its wall-clock duration to the last
    # flushed native record; this can omit at most one flush interval.
    $durationNs = [uint64](([datetime]$window.measurementEnd -
        [datetime]$window.measurementStart).TotalSeconds * 1000000000)
    $endNs = [uint64]$allRows[-1].present_end_ns
    $startNs = $endNs - $durationNs
    $rows = @($allRows | Where-Object { [uint64]$_.frame_start_ns -ge $startNs })
}
if ($rows.Count -lt 2) {
    throw "The measurement window contains fewer than two native frame records."
}

function Percentile([double[]] $Values, [double] $Percent) {
    $sorted = @($Values | Sort-Object)
    $index = [math]::Ceiling($Percent * $sorted.Count) - 1
    return $sorted[[math]::Max(0, $index)]
}
function Summary([string] $Name, [double[]] $Values) {
    [pscustomobject]@{
        metric = $Name
        samples = $Values.Count
        p50_ms = [math]::Round((Percentile $Values 0.50), 3)
        p90_ms = [math]::Round((Percentile $Values 0.90), 3)
        p95_ms = [math]::Round((Percentile $Values 0.95), 3)
        p99_ms = [math]::Round((Percentile $Values 0.99), 3)
        max_ms = [math]::Round(($Values | Measure-Object -Maximum).Maximum, 3)
    }
}
function CountSummary([string] $Name, [double[]] $Values) {
    [pscustomobject]@{
        metric = $Name
        samples = $Values.Count
        p50_count = [math]::Round((Percentile $Values 0.50), 3)
        p90_count = [math]::Round((Percentile $Values 0.90), 3)
        p95_count = [math]::Round((Percentile $Values 0.95), 3)
        p99_count = [math]::Round((Percentile $Values 0.99), 3)
        max_count = [math]::Round(($Values | Measure-Object -Maximum).Maximum, 3)
        total_count = [math]::Round(($Values | Measure-Object -Sum).Sum, 3)
    }
}

$frameIntervals = for ($index = 1; $index -lt $rows.Count; $index++) {
    ([uint64]$rows[$index].frame_start_ns - [uint64]$rows[$index - 1].frame_start_ns) / 1000000.0
}
$renderDurations = foreach ($row in $rows) {
    ([uint64]$row.render_submit_ns - [uint64]$row.frame_start_ns) / 1000000.0
}
$presentCallDurations = foreach ($row in $rows) {
    ([uint64]$row.present_end_ns - [uint64]$row.present_start_ns) / 1000000.0
}

$summaries = @(
    Summary "engine_frame_interval" $frameIntervals
    Summary "frame_start_to_render_submit" $renderDurations
    Summary "present_call_duration" $presentCallDurations
)

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
    foreach ($event in @("scene_tilemap_predraw", "tilemap_predraw_calc",
            "tilemap_border_rebuild",
            "bump_tile_calc_total",
            "bump_tile_texture_lod0_total", "bump_tile_texture_lod1_total",
            "bump_tile_texture_lod2_total", "bump_tile_texture_lod3_total",
            "bump_tile_texture_lod4_total",
            "bump_tile_mesh_total", "bump_tile_vertex_write_total",
            "bump_tile_point_region_total", "bump_tile_topology_total",
            "bump_tile_index_upload_total",
            "bump_tile_point_init_total", "bump_tile_region_scan_total",
            "tilemap_draw_bump",
            "d3d_submit_buffers_total")) {
        [double[]]$durations = @($workRows | Where-Object event -eq $event |
            ForEach-Object { ([uint64]$_.end_ns - [uint64]$_.start_ns) / 1000000.0 })
        if ($durations.Count -gt 0) { $summaries += Summary "engine_$event" $durations }
    }
    foreach ($event in @("bump_tile_calc_count", "tilemap_lod_cache_hit_count")) {
        [double[]]$counts = @($workRows | Where-Object event -eq $event |
            ForEach-Object { [double]$_.value })
        if ($counts.Count -gt 0) { $summaries += CountSummary "engine_$event" $counts }
    }
}

$dxvkPath = Join-Path $capture "dxvk-frame-timing.csv"
if (Test-Path -LiteralPath $dxvkPath -PathType Leaf) {
    $firstTimestamp = [uint64]$rows[0].frame_start_ns
    $lastTimestamp = [uint64]$rows[-1].present_end_ns
    $dxvkRows = @(Import-Csv -LiteralPath $dxvkPath | Where-Object {
        [uint64]$_.start_ns -ge $firstTimestamp -and [uint64]$_.start_ns -le $lastTimestamp
    })
    foreach ($event in @("queue_submit", "queue_present", "acquire_next_image",
            "wait_for_present_submission", "frame_latency_wait", "fps_limiter",
            "graphics_pipeline_compile", "compute_pipeline_compile")) {
        [double[]]$durations = @($dxvkRows | Where-Object event -eq $event |
            ForEach-Object { [uint64]$_.duration_ns / 1000000.0 })
        if ($durations.Count -gt 0) {
            $summaries += Summary "dxvk_$event" $durations
        }
    }

    $compileRows = @($dxvkRows | Where-Object {
        $_.event -eq "graphics_pipeline_compile" -or $_.event -eq "compute_pipeline_compile"
    })
    $longFrames = @($rows | Where-Object {
        ([uint64]$_.present_end_ns - [uint64]$_.frame_start_ns) -ge 37500000
    })
    $longFramesWithCompilation = @($longFrames | Where-Object {
        $frameStart = [uint64]$_.frame_start_ns
        $frameEnd = [uint64]$_.present_end_ns
        @($compileRows | Where-Object {
            [uint64]$_.start_ns -le $frameEnd -and [uint64]$_.end_ns -ge $frameStart
        }).Count -gt 0
    }).Count
}

$summaries | Format-Table -AutoSize

if (Test-Path -LiteralPath $dxvkPath -PathType Leaf) {
    Write-Host ""
    Write-Host ("Pipeline compilations in window: {0}; long frames overlapping compilation: {1}/{2}" -f `
        $compileRows.Count, $longFramesWithCompilation, $longFrames.Count)
}

$histogramBuckets = @(
    @{ label = "<25 ms"; min = [double]::NegativeInfinity; max = 25.0 },
    @{ label = "25-30 ms"; min = 25.0; max = 30.0 },
    @{ label = "30-37.5 ms"; min = 30.0; max = 37.5 },
    @{ label = "37.5-50 ms"; min = 37.5; max = 50.0 },
    @{ label = "50-66.7 ms"; min = 50.0; max = 66.7 },
    @{ label = ">=66.7 ms"; min = 66.7; max = [double]::PositiveInfinity }
)
Write-Host ""
Write-Host "Engine frame-interval histogram:"
$histogramBuckets | ForEach-Object {
    $bucket = $_
    $count = @($frameIntervals | Where-Object {
        $_ -ge $bucket.min -and $_ -lt $bucket.max
    }).Count
    [pscustomobject]@{
        interval = $bucket.label
        frames = $count
        percent = [math]::Round(100.0 * $count / $frameIntervals.Count, 2)
    }
} | Format-Table -AutoSize

Write-Host ""
Write-Host "These are engine and Present/swap-call timings, not compositor presentation timestamps."
