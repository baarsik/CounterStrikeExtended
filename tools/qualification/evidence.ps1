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
