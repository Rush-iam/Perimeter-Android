[CmdletBinding()]
param([Parameter(Mandatory)] [string] $CsvPath)

$rows = @(Import-Csv -LiteralPath $CsvPath | Sort-Object { [uint64]$_.actual_present_ns })
if ($rows.Count -lt 2) { throw "Fewer than two valid SurfaceFlinger frames were captured." }

# Some Android 10 vendors retain an older layer-history segment even after
# --latency-clear. Keep the largest continuous segment and discard gaps of one
# second or more, which cannot belong to this active 30/60 Hz capture.
$segments = [System.Collections.Generic.List[object]]::new()
$segment = [System.Collections.Generic.List[object]]::new()
foreach ($row in $rows) {
    if ($segment.Count -gt 0 -and
        ([uint64]$row.actual_present_ns - [uint64]$segment[-1].actual_present_ns) -ge 1000000000) {
        $segments.Add($segment.ToArray())
        $segment = [System.Collections.Generic.List[object]]::new()
    }
    $segment.Add($row)
}
$segments.Add($segment.ToArray())
$rows = @($segments | Sort-Object Count -Descending | Select-Object -First 1)
[object[]]$rows = $rows[0]
if ($rows.Count -lt 2) { throw "Fewer than two continuous SurfaceFlinger frames were captured." }
[double[]]$intervals = for ($i = 1; $i -lt $rows.Count; $i++) {
    ([uint64]$rows[$i].actual_present_ns - [uint64]$rows[$i - 1].actual_present_ns) / 1000000.0
}
function Percentile([double[]] $values, [double] $p) {
    $sorted = @($values | Sort-Object)
    $sorted[[math]::Max(0, [math]::Ceiling($p * $sorted.Count) - 1)]
}
[pscustomobject]@{
    frames = $rows.Count
    p50_ms = [math]::Round((Percentile $intervals 0.50), 3)
    p90_ms = [math]::Round((Percentile $intervals 0.90), 3)
    p95_ms = [math]::Round((Percentile $intervals 0.95), 3)
    p99_ms = [math]::Round((Percentile $intervals 0.99), 3)
    max_ms = [math]::Round(($intervals | Measure-Object -Maximum).Maximum, 3)
    under_25_ms = @($intervals | Where-Object { $_ -lt 25 }).Count
    target_25_to_42_ms = @($intervals | Where-Object { $_ -ge 25 -and $_ -lt 42 }).Count
    missed_42_ms_or_more = @($intervals | Where-Object { $_ -ge 42 }).Count
} | Format-List
