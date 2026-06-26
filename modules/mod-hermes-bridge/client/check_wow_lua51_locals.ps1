param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [int]$Limit = 200,
    [int]$WarnAt = 190
)

$ErrorActionPreference = 'Stop'

function Add-Token {
    param(
        [System.Collections.Generic.List[object]]$Tokens,
        [string]$Type,
        [string]$Text,
        [int]$Line
    )

    $Tokens.Add([pscustomobject]@{
        Type = $Type
        Text = $Text
        Line = $Line
    }) | Out-Null
}

function Read-LuaLongBracketEnd {
    param(
        [string]$Text,
        [int]$Index
    )

    if ($Index -ge $Text.Length -or $Text[$Index] -ne '[') {
        return $null
    }

    $cursor = $Index + 1
    while ($cursor -lt $Text.Length -and $Text[$cursor] -eq '=') {
        $cursor++
    }

    if ($cursor -lt $Text.Length -and $Text[$cursor] -eq '[') {
        return ']' + ('=' * ($cursor - $Index - 1)) + ']'
    }

    return $null
}

function Get-LuaTokens {
    param([string]$Source)

    $tokens = [System.Collections.Generic.List[object]]::new()
    $i = 0
    $line = 1

    while ($i -lt $Source.Length) {
        $ch = $Source[$i]

        if ($ch -eq "`r") {
            $i++
            continue
        }

        if ($ch -eq "`n") {
            Add-Token $tokens 'eol' "`n" $line
            $line++
            $i++
            continue
        }

        if ([char]::IsWhiteSpace($ch)) {
            $i++
            continue
        }

        if ($ch -eq '-' -and ($i + 1) -lt $Source.Length -and $Source[$i + 1] -eq '-') {
            $i += 2
            $longEnd = Read-LuaLongBracketEnd $Source $i
            if ($longEnd) {
                $i += 2 + ($longEnd.Length - 2)
                while ($i -lt $Source.Length) {
                    if ($i + $longEnd.Length -le $Source.Length -and $Source.Substring($i, $longEnd.Length) -eq $longEnd) {
                        $i += $longEnd.Length
                        break
                    }
                    if ($Source[$i] -eq "`n") {
                        Add-Token $tokens 'eol' "`n" $line
                        $line++
                    }
                    $i++
                }
            } else {
                while ($i -lt $Source.Length -and $Source[$i] -ne "`n") {
                    $i++
                }
            }
            continue
        }

        if ($ch -eq '[') {
            $longEnd = Read-LuaLongBracketEnd $Source $i
            if ($longEnd) {
                $i += 2 + ($longEnd.Length - 2)
                while ($i -lt $Source.Length) {
                    if ($i + $longEnd.Length -le $Source.Length -and $Source.Substring($i, $longEnd.Length) -eq $longEnd) {
                        $i += $longEnd.Length
                        break
                    }
                    if ($Source[$i] -eq "`n") {
                        Add-Token $tokens 'eol' "`n" $line
                        $line++
                    }
                    $i++
                }
                continue
            }
        }

        if ($ch -eq "'" -or $ch -eq '"') {
            $quote = $ch
            $i++
            while ($i -lt $Source.Length) {
                if ($Source[$i] -eq "`n") {
                    Add-Token $tokens 'eol' "`n" $line
                    $line++
                    $i++
                    continue
                }
                if ($Source[$i] -eq '\') {
                    $i += 2
                    continue
                }
                if ($Source[$i] -eq $quote) {
                    $i++
                    break
                }
                $i++
            }
            continue
        }

        if (($ch -ge 'A' -and $ch -le 'Z') -or ($ch -ge 'a' -and $ch -le 'z') -or $ch -eq '_') {
            $start = $i
            $i++
            while ($i -lt $Source.Length) {
                $c = $Source[$i]
                if (($c -ge 'A' -and $c -le 'Z') -or ($c -ge 'a' -and $c -le 'z') -or ($c -ge '0' -and $c -le '9') -or $c -eq '_') {
                    $i++
                } else {
                    break
                }
            }
            Add-Token $tokens 'ident' $Source.Substring($start, $i - $start) $line
            continue
        }

        Add-Token $tokens 'symbol' ([string]$ch) $line
        $i++
    }

    return $tokens
}

function Test-InsideFunction {
    param([object[]]$Scopes)

    foreach ($scope in $Scopes) {
        if ($scope.Type -eq 'function') {
            return $true
        }
    }

    return $false
}

$resolvedPath = (Resolve-Path -LiteralPath $Path).Path
$source = Get-Content -LiteralPath $resolvedPath -Raw
$tokens = Get-LuaTokens $source

$scopes = [System.Collections.ArrayList]::new()
$activeMainLocals = 0
$highWater = 0
$totalMainLocalDeclarations = 0
$mainLocalDeclarations = [System.Collections.Generic.List[object]]::new()
$skipDoCount = 0

function Add-MainLocals {
    param(
        [int]$Count,
        [int]$Line,
        [string]$Reason
    )

    if ($Count -le 0) {
        return
    }

    $script:activeMainLocals += $Count
    $script:totalMainLocalDeclarations += $Count
    if ($script:scopes.Count -gt 0) {
        $top = $script:scopes[$script:scopes.Count - 1]
        if ($top.MainScope) {
            $top.Locals += $Count
        }
    }
    if ($script:activeMainLocals -gt $script:highWater) {
        $script:highWater = $script:activeMainLocals
    }
    $script:mainLocalDeclarations.Add([pscustomobject]@{
        Line = $Line
        Count = $Count
        Active = $script:activeMainLocals
        Reason = $Reason
    }) | Out-Null
}

function Push-Scope {
    param(
        [string]$Type,
        [bool]$MainScope
    )

    $script:scopes.Add([pscustomobject]@{
        Type = $Type
        MainScope = $MainScope
        Locals = 0
    }) | Out-Null
}

function Pop-Scope {
    if ($script:scopes.Count -le 0) {
        return
    }

    $scope = $script:scopes[$script:scopes.Count - 1]
    $script:scopes.RemoveAt($script:scopes.Count - 1)
    if ($scope.MainScope) {
        $script:activeMainLocals -= $scope.Locals
        if ($script:activeMainLocals -lt 0) {
            $script:activeMainLocals = 0
        }
    }
}

for ($i = 0; $i -lt $tokens.Count; $i++) {
    $token = $tokens[$i]
    if ($token.Type -eq 'eol') {
        continue
    }

    $text = $token.Text
    $insideFunction = Test-InsideFunction $scopes

    if ($text -eq 'local') {
        if (-not $insideFunction) {
            $j = $i + 1
            while ($j -lt $tokens.Count -and $tokens[$j].Type -eq 'eol') {
                $j++
            }

            if ($j -lt $tokens.Count -and $tokens[$j].Text -eq 'function') {
                Add-MainLocals 1 $token.Line 'local function'
            } else {
                $count = 0
                while ($j -lt $tokens.Count) {
                    $candidate = $tokens[$j]
                    if ($candidate.Type -eq 'eol' -or $candidate.Text -eq '=') {
                        break
                    }
                    if ($candidate.Type -eq 'ident' -and $candidate.Text -ne 'local') {
                        $count++
                    }
                    $j++
                }
                Add-MainLocals $count $token.Line 'local declaration'
            }
        }
        continue
    }

    if ($text -eq 'function') {
        Push-Scope 'function' $false
        continue
    }

    if ($text -eq 'for') {
        $mainScope = -not $insideFunction
        Push-Scope 'for' $mainScope
        if ($mainScope) {
            $j = $i + 1
            $count = 0
            while ($j -lt $tokens.Count) {
                $candidate = $tokens[$j]
                if ($candidate.Type -eq 'eol' -or $candidate.Text -eq '=' -or $candidate.Text -eq 'in') {
                    break
                }
                if ($candidate.Type -eq 'ident') {
                    $count++
                }
                $j++
            }
            Add-MainLocals $count $token.Line 'for control variables'
        }
        $skipDoCount++
        continue
    }

    if ($text -eq 'while') {
        Push-Scope 'while' (-not $insideFunction)
        $skipDoCount++
        continue
    }

    if ($text -eq 'if') {
        Push-Scope 'if' (-not $insideFunction)
        continue
    }

    if ($text -eq 'repeat') {
        Push-Scope 'repeat' (-not $insideFunction)
        continue
    }

    if ($text -eq 'do') {
        if ($skipDoCount -gt 0) {
            $skipDoCount--
        } else {
            Push-Scope 'do' (-not $insideFunction)
        }
        continue
    }

    if ($text -eq 'end') {
        Pop-Scope
        continue
    }

    if ($text -eq 'until') {
        Pop-Scope
        continue
    }
}

$failed = $highWater -gt $Limit -or $totalMainLocalDeclarations -gt $Limit
$warned = $highWater -ge $WarnAt -or $totalMainLocalDeclarations -ge $WarnAt
$status = if ($failed) { 'FAIL' } elseif ($warned) { 'WARN' } else { 'OK' }
Write-Output ("{0}: {1} main chunk active locals high-water={2}, declared locals={3}, limit={4}, warnAt={5}" -f $status, $resolvedPath, $highWater, $totalMainLocalDeclarations, $Limit, $WarnAt)

if ($mainLocalDeclarations.Count -gt 0) {
    Write-Output 'Recent main local declarations near high-water:'
    $mainLocalDeclarations |
        Sort-Object Active -Descending |
        Select-Object -First 12 |
        Sort-Object Line |
        ForEach-Object {
            Write-Output ("  line {0}: +{1}, active={2}, {3}" -f $_.Line, $_.Count, $_.Active, $_.Reason)
        }
}

if ($failed) {
    exit 1
}
