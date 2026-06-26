param(
    [Parameter(Mandatory = $true)][string] $SourceDll,
    [Parameter(Mandatory = $true)][string] $ClientDir,
    [Parameter(Mandatory = $true)][string] $Version,
    [Parameter(Mandatory = $true)][string] $UniqueName
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $SourceDll)) {
    throw "Source DLL not found: $SourceDll"
}

if (-not (Test-Path -LiteralPath $ClientDir -PathType Container)) {
    throw "Client directory not found: $ClientDir"
}

$uniqueTarget = Join-Path $ClientDir $UniqueName
$stableTarget = Join-Path $ClientDir "HermesBridge.dll"
$markerPath = Join-Path $ClientDir "HermesBridge.client"

Copy-Item -LiteralPath $SourceDll -Destination $uniqueTarget -Force

try {
    Copy-Item -LiteralPath $SourceDll -Destination $stableTarget -Force
}
catch {
    [Console]::Error.WriteLine(("stable HermesBridge.dll copy skipped: {0}" -f $_.Exception.Message))
}

@(
    "version=$Version",
    "deployedAt=$([DateTime]::UtcNow.ToString('o'))",
    "uniqueDll=$UniqueName"
) | Set-Content -LiteralPath $markerPath -Encoding ASCII

[Console]::Write($uniqueTarget)
