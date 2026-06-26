param()

$ErrorActionPreference = "Stop"

$markerNames = @(
    "HermesBridge.client",
    "HermesBridge.dll"
)

function Get-WowDir([string] $path) {
    if (-not $path) {
        return $null
    }
    return [System.IO.Path]::GetDirectoryName($path)
}

function Has-HermesMarker([string] $dir) {
    if (-not $dir) {
        return $false
    }

    foreach ($name in $markerNames) {
        if (Test-Path -LiteralPath (Join-Path $dir $name)) {
            return $true
        }
    }

    return @((Get-ChildItem -LiteralPath $dir -Filter "HermesBridge_*.dll" -File -ErrorAction SilentlyContinue)).Count -gt 0
}

$processes = @(Get-Process wow -ErrorAction SilentlyContinue | Where-Object { $_.Path })

if ($env:HERMES_WOW_PID) {
    $processes = @($processes | Where-Object { $_.Id -eq [int]$env:HERMES_WOW_PID })
}

if ($env:HERMES_WOW_EXE_PATH) {
    $targetPath = [System.IO.Path]::GetFullPath($env:HERMES_WOW_EXE_PATH)
    $processes = @($processes | Where-Object {
        [string]::Equals([System.IO.Path]::GetFullPath($_.Path), $targetPath, [System.StringComparison]::OrdinalIgnoreCase)
    })
}

if ($env:HERMES_WOW_DIR) {
    $targetDir = [System.IO.Path]::GetFullPath($env:HERMES_WOW_DIR).TrimEnd('\', '/')
    $processes = @($processes | Where-Object {
        $dir = Get-WowDir $_.Path
        $dir -and [string]::Equals([System.IO.Path]::GetFullPath($dir).TrimEnd('\', '/'), $targetDir, [System.StringComparison]::OrdinalIgnoreCase)
    })
}

if ($processes.Count -eq 0) {
    Write-Error "wow.exe not found; start the client first"
    exit 1
}

$marked = @($processes | Where-Object { Has-HermesMarker (Get-WowDir $_.Path) })

if ($marked.Count -eq 1) {
    $selected = $marked[0]
}
elseif ($marked.Count -gt 1) {
    $selected = $marked | Sort-Object WorkingSet64 -Descending | Select-Object -First 1
}
elseif ($processes.Count -eq 1) {
    $selected = $processes[0]
}
else {
    Write-Error "multiple wow.exe processes found and no HermesBridge marker matched; set HERMES_WOW_PID or HERMES_WOW_DIR"
    $processes | ForEach-Object { Write-Error ("candidate pid={0} path={1}" -f $_.Id, $_.Path) }
    exit 2
}

$selectedDir = Get-WowDir $selected.Path
[Console]::Write(("{0}|{1}|{2}" -f $selected.Id, $selected.Path, $selectedDir))
