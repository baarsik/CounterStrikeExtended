<#
Run on the interactive Windows desktop, for example through vm1.py --interactive.
SandboxDirectory must be empty or carry this tool's ownership marker. Each run
uses a unique TestCaseLabel/EvidenceDirectory; this script never changes VM shares.
Validate the copied .dem afterward with validate_demo.py and the live/movement gates.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('cstrike', 'valve')][string]$Game,
    [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
    [Parameter(Mandatory=$true)][string]$GameDataDirectory,
    [Parameter(Mandatory=$true)][string]$SandboxDirectory,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [int]$SteamAppId = 0,
    [string]$SourceCommit = '',
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$TestCaseLabel = 'baseline',
    [string]$ModuleCollectorPath = '',
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'evidence.ps1')

function Get-FullDirectory([string]$Path) {
    return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-ContainsPath([string]$Parent, [string]$Child) {
    return $Child.Equals($Parent, [StringComparison]::OrdinalIgnoreCase) -or
        $Child.StartsWith($Parent + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Copy-Tree([string]$Source, [string]$Destination) {
    & robocopy $Source $Destination /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "Runtime/data overlay failed ($LASTEXITCODE): $Source"
    }
}

function Get-RecordedNetworkFrames([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ($stream.Length -lt 544) { throw 'Demo header is truncated' }
        $stamp = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(8))
        if (-not $stamp.StartsWith('HLDEMO') -or $reader.ReadInt32() -ne 5 -or $reader.ReadInt32() -ne 48) {
            throw 'Demo header/protocol is invalid'
        }
        $stream.Position = 540
        $directory = $reader.ReadInt32()
        if ($directory -lt 544 -or $directory -gt $stream.Length - 4) { throw 'Demo is not finalized' }
        $stream.Position = $directory
        $entries = $reader.ReadInt32()
        if ($entries -lt 1 -or $entries -gt 1024 -or $directory + 4 + 92 * $entries -ne $stream.Length) {
            throw 'Demo directory is invalid'
        }
        $frames = 0
        for ($index = 0; $index -lt $entries; $index++) {
            $kind = $reader.ReadInt32()
            $stream.Position += 76
            $count = $reader.ReadInt32()
            $offset = $reader.ReadInt32()
            $length = $reader.ReadInt32()
            if ($count -lt 0 -or $offset -lt 544 -or $length -lt 9 -or $offset + $length -gt $directory) {
                throw 'Demo track bounds/count are invalid'
            }
            if ($kind -eq 1) { $frames += $count }
        }
        if ($frames -lt 100) { throw "Recording has insufficient network frames: $frames" }
        return $frames
    } finally {
        $reader.Dispose()
    }
}

$RuntimeDirectory = Get-FullDirectory $RuntimeDirectory
$GameDataDirectory = Get-FullDirectory $GameDataDirectory
$SandboxDirectory = Get-FullDirectory $SandboxDirectory
$EvidenceDirectory = Get-FullDirectory $EvidenceDirectory
foreach ($source in @($RuntimeDirectory, $GameDataDirectory, $EvidenceDirectory)) {
    if ((Test-ContainsPath $SandboxDirectory $source) -or (Test-ContainsPath $source $SandboxDirectory)) {
        throw 'Sandbox must be separate from source/runtime/evidence directories'
    }
}
foreach ($source in @($RuntimeDirectory, $GameDataDirectory)) {
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing source directory: $source" }
}
$marker = Join-Path $SandboxDirectory '.cse-qualification-sandbox'
if ((Test-Path -LiteralPath $SandboxDirectory) -and -not (Test-Path -LiteralPath $marker)) {
    if (@(Get-ChildItem -LiteralPath $SandboxDirectory -Force).Count -ne 0) {
        throw 'Refusing a nonempty sandbox without the qualification ownership marker'
    }
}
New-Item -ItemType Directory -Path $SandboxDirectory, $EvidenceDirectory -Force | Out-Null
[IO.File]::WriteAllText($marker, 'Owned qualification sandbox; runtime overlay and probe outputs only.')
$prefix = "$Game-$TestCaseLabel"
$resultPath = Join-Path $EvidenceDirectory "$prefix-result.json"
Reserve-ProbeEvidence $EvidenceDirectory $prefix | Out-Null
if ($SteamAppId -eq 0) {
    $SteamAppId = if ($Game -eq 'cstrike') { 10 } else { 70 }
}
if ($ModuleCollectorPath -and -not (Test-Path -LiteralPath $ModuleCollectorPath -PathType Leaf)) {
    throw 'Module collector does not exist'
}
$gameExecutable = Join-Path $SandboxDirectory 'hl.exe'
$existing = @(Get-CimInstance Win32_Process -Filter "Name='hl.exe'" |
    Where-Object { $_.ExecutablePath -and $_.ExecutablePath.Equals($gameExecutable, [StringComparison]::OrdinalIgnoreCase) })
if ($existing.Count -ne 0) { throw 'An existing game process is using this sandbox; refusing to overlay it' }
$process = $null
$failure = $null
$modulesCollected = $false
$oldSteamAppId = $env:SteamAppId
$oldSteamGameId = $env:SteamGameId
$result = [ordered]@{
    success = $false; game = $Game; testcase = $TestCaseLabel; source_commit = $SourceCommit
    steam_app_id = $SteamAppId; runtime = $RuntimeDirectory; game_data = $GameDataDirectory
    sandbox = $SandboxDirectory; started_utc = [DateTime]::UtcNow.ToString('o'); pid = $null
    exit_code = $null; module_collection_requested = [bool]$ModuleCollectorPath
}
$gameDirectory = Join-Path $SandboxDirectory $Game
$demoPath = Join-Path $gameDirectory 'cseprobe.dem'
$logs = @((Join-Path $SandboxDirectory 'qconsole.log'), (Join-Path $gameDirectory 'qconsole.log'))
try {
    # Full runtime-x86 output contract; a hardware-only directory must not borrow stock DLLs.
    $requiredRuntime = @('hl.exe', 'hw.dll', 'filesystem_stdio.dll', 'vgui.dll', 'vgui2.dll', 'tier0.dll',
        'Core.dll', 'DemoPlayer.dll', 'hlds.exe', 'swds.dll', 'valve\cl_dlls\GameUI.dll',
        'valve\cl_dlls\particleman.dll', 'valve\cl_dlls\client.dll', 'valve\dlls\hl.dll',
        'cstrike\cl_dlls\client.dll', 'cstrike\dlls\mp.dll')
    $runtimeFiles = @(foreach ($relative in $requiredRuntime) {
        $path = Join-Path $RuntimeDirectory $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing full-runtime output: $relative" }
        [ordered]@{ relative_path = $relative; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
    Copy-Tree $GameDataDirectory $SandboxDirectory
    Copy-Tree $RuntimeDirectory $SandboxDirectory
    foreach ($file in $runtimeFiles) {
        $actual = (Get-FileHash -LiteralPath (Join-Path $SandboxDirectory $file.relative_path) -Algorithm SHA256).Hash
        if ($actual -ne $file.sha256) { throw "Sandbox runtime overlay mismatch: $($file.relative_path)" }
    }
    $runtimeFiles | ConvertTo-Json -Depth 3 |
        Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$prefix-runtime-files.json")
    foreach ($path in @($logs) + @($demoPath)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }
    # Screenshots are created only inside the owned sandbox, never the data source.
    Get-ChildItem -LiteralPath $gameDirectory -Filter 'HalfLife*.webp' -File | Remove-Item -Force
    $map = if ($Game -eq 'cstrike') { 'de_dust2' } else { 'crossfire' }
    $commands = @('developer 1', 'sv_lan 1', 'sv_cheats 1', 'fps_override 0', 'fps_max 100', 'gl_vsync 0', "map $map")
    $commands += @('wait') * 500
    if ($Game -eq 'cstrike') {
        $commands += @('mp_freezetime 0', 'mp_limitteams 0', 'mp_autoteambalance 0', 'jointeam 2', 'joinclass 1')
    }
    $commands += @('wait') * 200
    $commands += @('echo CSE_PLAYER_READY', 'screenshot', 'status', 'spec_pos', 'record cseprobe', '+forward')
    $commands += @('wait') * 300
    $commands += @('-forward', 'spec_pos', '+right')
    $commands += @('wait') * 200
    $commands += @('-right', '+attack')
    $commands += @('wait') * 150
    $commands += @('-attack', 'stop', 'echo CSE_RECORDING_DONE', 'timedemo cseprobe')
    # 500 waits prematurely cut off the old probe at 499 replay frames.
    $commands += @('wait') * 1200
    $commands += @('echo CSE_PROBE_DONE', 'quit')
    $config = ($commands -join "`n") + "`n"
    $configBytes = [Text.Encoding]::ASCII.GetByteCount($config)
    if ($configBytes -ge 16384) { throw "Probe exceeds command buffer: $configBytes bytes" }
    [IO.File]::WriteAllText((Join-Path $gameDirectory 'cseprobe.cfg'), $config, [Text.Encoding]::ASCII)
    $result.config_bytes = $configBytes
    $env:SteamAppId = [string]$SteamAppId
    $env:SteamGameId = [string]$SteamAppId
    $arguments = "-steam -game $Game -maxplayers 2 -gl -windowed -forceres -w 640 -h 480 -novid -dev -console -condebug -insecure -nomaster +sv_lan 1 +ip 127.0.0.1 +exec cseprobe.cfg"
    $result.arguments = $arguments
    $process = Start-Process $gameExecutable -WorkingDirectory $SandboxDirectory -ArgumentList $arguments -PassThru
    $result.pid = $process.Id
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $result | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$prefix-start.json")
    while (-not $process.WaitForExit(100)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw "Owned game PID $($process.Id) timed out" }
        if ($ModuleCollectorPath -and -not $modulesCollected) {
            $ready = $false
            foreach ($log in $logs) {
                if (Test-Path -LiteralPath $log) {
                    $content = Get-Content -LiteralPath $log -Raw
                    if ($null -ne $content -and $content.Contains('CSE_PLAYER_READY')) { $ready = $true }
                }
            }
            if ($ready) {
                $rawModules = Join-Path $EvidenceDirectory "$prefix-modules.txt"
                & $ModuleCollectorPath $process.Id $rawModules
                if ($LASTEXITCODE -ne 0) { throw "Native module collection failed: $LASTEXITCODE" }
                $modules = @(foreach ($path in Get-Content -LiteralPath $rawModules -Encoding UTF8) {
                    $modulePath = [string]$path
                    [ordered]@{
                        path = $modulePath; file_version = (Get-Item -LiteralPath $modulePath).VersionInfo.FileVersion
                        sha256 = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash.ToLowerInvariant()
                    }
                })
                if ($modules.Count -eq 0) { throw 'Module collector returned no modules' }
                [ordered]@{ pid = $process.Id; collected_utc = [DateTime]::UtcNow.ToString('o'); modules = $modules } |
                    ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$prefix-modules.json")
                $modulesCollected = $true
            }
        }
    }
    $result.exit_code = $process.ExitCode
    if ($process.ExitCode -ne 0) { throw "Owned game exited with code $($process.ExitCode)" }
    if ($ModuleCollectorPath -and -not $modulesCollected) { throw 'Live module collection did not reach readiness' }
    $console = ''
    foreach ($log in $logs) {
        if (Test-Path -LiteralPath $log) { $console += Get-Content -LiteralPath $log -Raw }
    }
    if ($console -notmatch '(?s)CSE_PLAYER_READY.*CSE_RECORDING_DONE.*Host_EndGame:\s*End of demo\..*CSE_PROBE_DONE') {
        throw 'Missing full recording/replay completion sequence; exit zero alone is insufficient'
    }
    $expectedFrames = Get-RecordedNetworkFrames $demoPath
    $playback = $console.Substring($console.IndexOf('CSE_RECORDING_DONE'))
    $frameReports = [regex]::Matches($playback, '(?m)^(\d+) frames\s+[\d.]+ seconds\s+[\d.]+ fps')
    if ($frameReports.Count -ne 1) { throw 'Expected exactly one timedemo frame report' }
    $observedFrames = [int]$frameReports[0].Groups[1].Value
    # CL_FinishTimeDemo excludes its first host frame; End of demo proves EOF.
    if ($observedFrames -lt $expectedFrames - 1) {
        throw "Partial replay: $observedFrames reported frames for $expectedFrames recorded network frames"
    }
    $result.recorded_network_frames = $expectedFrames
    $result.replay_reported_host_frames = $observedFrames
    $result.replay_reached_end = $true
    $result.demo_sha256 = (Get-FileHash -LiteralPath $demoPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $result.success = $true
} catch {
    $failure = $_.Exception.Message
} finally {
    if ($process -and -not $process.HasExited) {
        try {
            [void]$process.CloseMainWindow()
            if (-not $process.WaitForExit(3000)) {
                $process.Kill()
                if (-not $process.WaitForExit(3000)) { throw 'Owned process did not terminate' }
            }
            $result.forced_cleanup = $true
        } catch {
            $failure = "$failure; owned-PID cleanup failed: $($_.Exception.Message)"
        }
    }
    $env:SteamAppId = $oldSteamAppId
    $env:SteamGameId = $oldSteamGameId
    try {
        $logIndex = 0
        foreach ($log in $logs) {
            if (Test-Path -LiteralPath $log) {
                Copy-Item -LiteralPath $log -Destination (Join-Path $EvidenceDirectory "$prefix-qconsole-$logIndex.log")
            }
            $logIndex++
        }
        if (Test-Path -LiteralPath $demoPath) {
            Copy-Item -LiteralPath $demoPath -Destination (Join-Path $EvidenceDirectory "$prefix.dem")
        }
        if (Test-Path -LiteralPath $gameDirectory) {
            Get-ChildItem -LiteralPath $gameDirectory -Filter 'HalfLife*.webp' -File |
                ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $EvidenceDirectory "$prefix-$($_.Name)") }
        }
    } catch {
        $failure = "$failure; evidence copy failed: $($_.Exception.Message)"
    }
    if ($failure) { $result.success = $false; $result.failure = $failure }
    $result.finished_utc = [DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $resultPath
    $result | ConvertTo-Json -Depth 5
}
if ($failure) { throw $failure }
