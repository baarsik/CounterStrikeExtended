function Reserve-ProbeEvidence([string]$Directory, [string]$Prefix) {
    $artifacts = @(Get-ChildItem -LiteralPath $Directory -Force | Where-Object {
        $_.Name.StartsWith($Prefix + '-', [StringComparison]::OrdinalIgnoreCase) -or
            $_.Name.Equals($Prefix + '.dem', [StringComparison]::OrdinalIgnoreCase)
    })
    if ($artifacts.Count -ne 0) { throw 'Evidence prefix already exists; choose a new testcase label/directory' }
    $path = Join-Path $Directory "$Prefix-reserved.json"
    # CreateNew closes the race between concurrent callers which both saw no artifacts.
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $text = @{ prefix = $Prefix; reserved_utc = [DateTime]::UtcNow.ToString('o') } | ConvertTo-Json
        $bytes = [Text.Encoding]::UTF8.GetBytes($text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally {
        $stream.Dispose()
    }
    return $path
}

function Read-SharedProbeLog([string]$Path) {
    $stream = $null
    $reader = $null
    try {
        $sharing = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, $sharing)
        $reader = New-Object IO.StreamReader($stream)
        return $reader.ReadToEnd()
    } catch [IO.IOException] {
        # Creation/rotation may race a poll; the next observation can retry.
        return ''
    } finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}
