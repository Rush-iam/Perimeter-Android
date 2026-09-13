[CmdletBinding()]
param(
    [string] $Serial,
    [string] $Adb,
    [ValidateRange(0, 120)]
    [int] $TransitionSeconds = 3,

    [ValidateRange(0, 120)]
    [int] $InitialMenuSeconds = 20,
    [ValidateRange(0, 180)]
    [int] $MissionLoadSeconds = 80
)

$ErrorActionPreference = "Stop"
if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA "Android/Sdk/platform-tools/adb.exe"
}
if (-not (Test-Path -LiteralPath $Adb -PathType Leaf)) {
    throw "adb was not found at '$Adb'. Pass -Adb with the Android platform-tools path."
}

$adbPrefix = if ($Serial) { @("-s", $Serial) } else { @() }
function Invoke-Adb {
    param([Parameter(ValueFromRemainingArguments)] [string[]] $Arguments)
    & $Adb @adbPrefix @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed ($LASTEXITCODE): $($Arguments -join ' ')"
    }
}

$windowState = (Invoke-Adb shell dumpsys window displays) -join "`n"
$logicalSize = [regex]::Match($windowState, "cur=(\d+)x(\d+)")
if (-not $logicalSize.Success) {
    throw "Could not determine the active logical display size."
}
$width = [int]$logicalSize.Groups[1].Value
$height = [int]$logicalSize.Groups[2].Value
if ($width -lt $height) {
    throw "Open Perimeter's landscape main menu before running this helper."
}

function Tap-Normalized([double] $X, [double] $Y, [string] $Label) {
    $pixelX = [math]::Round($width * $X)
    $pixelY = [math]::Round($height * $Y)
    Write-Host "Tap $Label at ($pixelX,$pixelY)"
    Invoke-Adb shell input tap $pixelX $pixelY
}

# Allow the game main menu to settle before the first tap. This helper starts
# after ContentActivity's Play control has launched the game itself.
# The delay is intentionally independent from the short between-menu transition.
if ($InitialMenuSeconds -gt 0) {
    Write-Host "Waiting $InitialMenuSeconds seconds for the main menu"
    Start-Sleep -Seconds $InitialMenuSeconds
}

# Coordinates re-mapped from the 2560x1600 main, Single Player, and Campaign
# screens on 2026-09-10. Use the centre of each target: the prior first and Go
# coordinates were too close to button boundaries and could fall through to
# Multiplayer or miss Go. Tutorial must be explicitly activated before Go.
Tap-Normalized 0.5000 0.4188 "Single Player"
Start-Sleep -Seconds $TransitionSeconds
Tap-Normalized 0.5000 0.5125 "Campaign"
Start-Sleep -Seconds $TransitionSeconds
Tap-Normalized 0.5000 0.2438 "Tutorial"
Start-Sleep -Milliseconds 500
Tap-Normalized 0.7656 0.9156 "Go"

Write-Host "Waiting $MissionLoadSeconds seconds for loading and the scripted opening."
Start-Sleep -Seconds $MissionLoadSeconds
Write-Host "Confirm the normal gameplay HUD is visible before starting capture."
