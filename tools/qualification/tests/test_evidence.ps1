$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'evidence.ps1')
$root = Join-Path ([IO.Path]::GetTempPath()) ('CSE-evidence-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    foreach ($suffix in @('start.json', 'result.json')) {
        $directory = Join-Path $root $suffix
        New-Item -ItemType Directory -Path $directory | Out-Null
        $existing = Join-Path $directory "cstrike-case-$suffix"
        [IO.File]::WriteAllText($existing, 'original evidence')
        $rejected = $false
        try { Reserve-ProbeEvidence $directory 'cstrike-case' | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw "Prefix reuse accepted with existing $suffix" }
        if ([IO.File]::ReadAllText($existing) -ne 'original evidence') { throw 'Existing evidence changed' }
        if (@(Get-ChildItem -LiteralPath $directory).Count -ne 1) { throw 'Rejected run created new evidence' }
    }
    $directory = Join-Path $root 'reservation'
    New-Item -ItemType Directory -Path $directory | Out-Null
    $reservation = Reserve-ProbeEvidence $directory 'valve-case'
    if (-not (Test-Path -LiteralPath $reservation)) { throw 'Reservation was not persisted' }
    $original = [IO.File]::ReadAllText($reservation)
    $rejected = $false
    try { Reserve-ProbeEvidence $directory 'valve-case' | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Existing reservation was accepted twice' }
    if ([IO.File]::ReadAllText($reservation) -ne $original) { throw 'Reservation changed on rejected retry' }
    $log = Join-Path $root 'live-console.log'
    $writer = [IO.File]::Open($log, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
    try {
        $bytes = [Text.Encoding]::ASCII.GetBytes('CSE_MUSIC_FULL')
        $writer.Write($bytes, 0, $bytes.Length)
        $writer.Flush()
        if ((Read-SharedProbeLog $log) -ne 'CSE_MUSIC_FULL') { throw 'Live console could not be read while open for writing' }
    } finally { $writer.Dispose() }
    Write-Output 'PASS: interrupted, completed and already-reserved evidence prefixes are preserved'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
