<# Interactive VM1 only. Sandboxes must already be staged and qualified; no provider collection occurs during replay. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('cstrike', 'valve')][string]$Game,
    [Parameter(Mandatory=$true)][string]$BaselineSandboxDirectory,
    [Parameter(Mandatory=$true)][string]$CandidateSandboxDirectory,
    [Parameter(Mandatory=$true)][string]$DemoPath,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$DemoSha256,
    [Parameter(Mandatory=$true)][string]$SettingsPath,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$TestCaseLabel,
    [string]$BaselineCommit = '',
    [string]$CandidateCommit = '',
    [ValidateRange(1, 30)][int]$Pairs = 7,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'evidence.ps1')
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CseBenchmarkWindow {
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
}
'@
function Get-NativeManifest([string]$Directory) {
    return @(Get-ChildItem -LiteralPath $Directory -Recurse -File | Where-Object {
        $_.Extension -in @('.exe', '.dll', '.asi', '.drv')
    } | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($Directory.Length + 1); sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash }
    })
}
$BaselineSandboxDirectory = [IO.Path]::GetFullPath($BaselineSandboxDirectory).TrimEnd('\')
$CandidateSandboxDirectory = [IO.Path]::GetFullPath($CandidateSandboxDirectory).TrimEnd('\')
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd('\')
$directories = @($BaselineSandboxDirectory, $CandidateSandboxDirectory, $EvidenceDirectory)
if (@($directories | ForEach-Object { $_.ToLowerInvariant() } | Sort-Object -Unique).Count -ne 3) {
    throw 'Sandbox and evidence directories must be three distinct paths'
}
foreach ($left in $directories) {
    foreach ($right in $directories) {
        if ($left -ne $right -and $left.StartsWith($right + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Sandbox and evidence directories must not contain each other'
        }
    }
}
if ($BaselineSandboxDirectory.Equals($CandidateSandboxDirectory, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Baseline and candidate sandboxes must differ'
}
if (@(Get-Process hl, hlds -ErrorAction SilentlyContinue).Count) { throw 'Close other game/server processes first' }
foreach ($sandbox in @($BaselineSandboxDirectory, $CandidateSandboxDirectory)) {
    if (-not (Test-Path -LiteralPath (Join-Path $sandbox '.cse-qualification-sandbox'))) {
        throw "Missing qualification ownership marker: $sandbox"
    }
    foreach ($relative in @('hl.exe', 'hw.dll', "$Game\liblist.gam")) {
        if (-not (Test-Path -LiteralPath (Join-Path $sandbox $relative) -PathType Leaf)) { throw "Missing $relative" }
    }
}
$acceptedDemo = if ($Game -eq 'cstrike') {
    'fb1a18edf31182d49b022a46850df0cd263fe1ea101b28caac78af038fdf211c'
} else {
    '3177530c2d13a5e88f2ffc4f86f0baefe939034cde0b5c8d787583f4c035ffa3'
}
if ($DemoSha256 -ne $acceptedDemo -or (Get-FileHash -LiteralPath $DemoPath).Hash -ne $DemoSha256) {
    throw 'Fixed demo hash/game mismatch'
}
$settings = [IO.File]::ReadAllBytes($SettingsPath)
if (@($settings | Where-Object { $_ -gt 127 }).Count) { throw 'Settings must be ASCII console commands' }
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$seriesPrefix = "$Game-$TestCaseLabel"
Reserve-ProbeEvidence $EvidenceDirectory $seriesPrefix | Out-Null
[IO.File]::WriteAllBytes((Join-Path $EvidenceDirectory "$seriesPrefix-settings.cfg"), $settings)
$manifestBefore = @{}
foreach ($version in @('baseline', 'candidate')) {
    $sandbox = if ($version -eq 'baseline') { $BaselineSandboxDirectory } else { $CandidateSandboxDirectory }
    $manifestBefore[$version] = Get-NativeManifest $sandbox
    if ($version -eq 'candidate' -and @($manifestBefore[$version] | Where-Object {
        $_.path -match '(?i)(^|[\\/])(SDL2[^\\/]*|sdl2-compat[^\\/]*)\.(dll|exe)$'
    }).Count) { throw 'Candidate contains SDL2/compatibility artifacts' }
}
# Engine/client outputs and the SDL provider intentionally differ; every other native provider must match.
$versionedOutputs = @('hl.exe', 'hw.dll', 'filesystem_stdio.dll', 'vgui.dll', 'vgui2.dll', 'tier0.dll',
    'Core.dll', 'DemoPlayer.dll', 'hlds.exe', 'swds.dll', 'valve\cl_dlls\GameUI.dll',
    'valve\cl_dlls\particleman.dll', 'valve\cl_dlls\client.dll', 'valve\dlls\hl.dll',
    'cstrike\cl_dlls\client.dll', 'cstrike\dlls\mp.dll', 'a3dapi.dll', 'hltv.exe', 'proxy.dll', 'SDL2.dll', 'SDL3.dll')
$sharedProviders = @{}
foreach ($version in @('baseline', 'candidate')) {
    $sharedProviders[$version] = @($manifestBefore[$version] | Where-Object { $_.path -notin $versionedOutputs })
}
if (($sharedProviders.baseline | ConvertTo-Json -Depth 4 -Compress) -ne
    ($sharedProviders.candidate | ConvertTo-Json -Depth 4 -Compress)) {
    throw 'Native non-migration providers differ; stage matching approved providers in both sandboxes'
}
# Hash identical map/resource inputs outside process lifetime; do not compare unrelated mutable save/config files.
$assets = @{}
$map = if ($Game -eq 'cstrike') { 'de_dust2' } else { 'crossfire' }
foreach ($version in @('baseline', 'candidate')) {
    $sandbox = if ($version -eq 'baseline') { $BaselineSandboxDirectory } else { $CandidateSandboxDirectory }
    $paths = @(Get-ChildItem -LiteralPath (Join-Path $sandbox 'valve'), (Join-Path $sandbox $Game) -File -Recurse |
        Where-Object { $_.Extension -in @('.pak', '.wad') -or $_.Name -eq "$map.bsp" } |
        Sort-Object FullName -Unique)
    $assets[$version] = @($paths | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($sandbox.Length + 1); sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash }
    })
    if (-not @($assets[$version] | Where-Object { $_.path -eq "$Game\maps\$map.bsp" }).Count) {
        throw "Missing fixed benchmark map: $map"
    }
}
if (($assets.baseline | ConvertTo-Json -Depth 4 -Compress) -ne ($assets.candidate | ConvertTo-Json -Depth 4 -Compress)) {
    throw 'Fixed map/PAK/WAD inputs differ'
}
$series = [ordered]@{
    schema_version = 1; game = $Game; label = $TestCaseLabel; pairs = $Pairs
    baseline_commit = $BaselineCommit; candidate_commit = $CandidateCommit
    demo_sha256 = $DemoSha256.ToLowerInvariant(); settings_sha256 = (Get-FileHash -LiteralPath $SettingsPath).Hash
    baseline_sandbox = $BaselineSandboxDirectory; candidate_sandbox = $CandidateSandboxDirectory
    instrumentation = 'host_speeds integer FPS + condebug; 100 ms owned-window observation; no live module/hash/log polling'
    native_before = $manifestBefore; assets = $assets; success = $false; runs = @()
}
$seriesPath = Join-Path $EvidenceDirectory "$seriesPrefix-series.json"
$series | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $seriesPath
$oldApp = $env:SteamAppId
$oldGame = $env:SteamGameId
try {
    $env:SteamAppId = if ($Game -eq 'cstrike') { '10' } else { '70' }
    $env:SteamGameId = $env:SteamAppId
    for ($pair = 0; $pair -le $Pairs; $pair++) {
        $order = if ($pair % 2 -eq 0) { @('baseline', 'candidate') } else { @('candidate', 'baseline') }
        foreach ($version in $order) {
            $runPrefix = "$seriesPrefix-p$pair-$version"
            Reserve-ProbeEvidence $EvidenceDirectory $runPrefix | Out-Null
            $sandbox = if ($version -eq 'baseline') { $BaselineSandboxDirectory } else { $CandidateSandboxDirectory }
            $gameDirectory = Join-Path $sandbox $Game
            $logs = @((Join-Path $sandbox 'qconsole.log'), (Join-Path $gameDirectory 'qconsole.log'))
            foreach ($log in $logs) { if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force } }
            Copy-Item -LiteralPath $DemoPath -Destination (Join-Path $gameDirectory 'csefixed.dem') -Force
            [IO.File]::WriteAllBytes((Join-Path $gameDirectory 'config.cfg'), $settings)
            [IO.File]::WriteAllText((Join-Path $gameDirectory 'autoexec.cfg'), '', [Text.Encoding]::ASCII)
            # Explicitly execute the same settings again after startup; command line fixes viewport and player mode.
            [IO.File]::WriteAllBytes((Join-Path $gameDirectory 'csebench-settings.cfg'), $settings)
            $commands = @('exec csebench-settings.cfg', 'developer 1', 'fps_override 0', 'fps_max 100', 'gl_vsync 0', 'host_speeds 0')
            $commands += @('wait') * 300
            $commands += @('echo CSE_BENCH_START', 'host_speeds 1', 'timedemo csefixed')
            $commands += @('wait') * 1200
            $commands += @('host_speeds 0', 'echo CSE_BENCH_DONE', 'quit')
            [IO.File]::WriteAllText((Join-Path $gameDirectory 'csebench.cfg'), ($commands -join "`n") + "`n", [Text.Encoding]::ASCII)
            $arguments = "-steam -game $Game -maxplayers 2 -gl -windowed -forceres -w 640 -h 480 -novid -dev -console -condebug -insecure -nomaster +sv_lan 1 +ip 127.0.0.1 +exec csebench.cfg"
            $result = [ordered]@{
                schema_version = 1; game = $Game; version = $version; pair = $pair; warmup = ($pair -eq 0)
                prefix = $runPrefix; series = "$seriesPrefix-series.json"; arguments = $arguments
                demo_sha256 = $DemoSha256.ToLowerInvariant(); success = $false; pid = $null; exit_code = $null
                window_samples = @(); started_utc = [DateTime]::UtcNow.ToString('o')
            }
            $process = $null
            $failure = $null
            $timer = [Diagnostics.Stopwatch]::StartNew()
            try {
                $process = Start-Process (Join-Path $sandbox 'hl.exe') -WorkingDirectory $sandbox -ArgumentList $arguments -PassThru
                $result.pid = $process.Id
                $hadForeground = $false
                while (-not $process.WaitForExit(100)) {
                    if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Owned benchmark process timed out' }
                    $process.Refresh()
                    $window = $process.MainWindowHandle
                    $visible = $window -ne [IntPtr]::Zero -and [CseBenchmarkWindow]::IsWindowVisible($window)
                    $minimized = $window -ne [IntPtr]::Zero -and [CseBenchmarkWindow]::IsIconic($window)
                    [uint32]$foregroundPid = 0
                    [void][CseBenchmarkWindow]::GetWindowThreadProcessId([CseBenchmarkWindow]::GetForegroundWindow(), [ref]$foregroundPid)
                    $foreground = $visible -and -not $minimized -and $foregroundPid -eq $process.Id
                    $result.window_samples += [ordered]@{
                        elapsed_ms = $timer.ElapsedMilliseconds; visible = $visible; minimized = $minimized
                        foreground_pid = $foregroundPid; owned_foreground = $foreground
                    }
                    if ($foreground) { $hadForeground = $true }
                    if ($hadForeground -and $visible -and -not $foreground) { throw 'Owned visible window lost foreground' }
                }
                $timer.Stop()
                $result.exit_code = $process.ExitCode
                if ($process.ExitCode -ne 0) { throw "Benchmark exit code $($process.ExitCode)" }
                if (-not $hadForeground) { throw 'No visible foreground owned game window observed' }
                # Permit only the final 250 ms of window destruction during clean process shutdown.
                $foregroundAcquired = $false
                foreach ($sample in $result.window_samples) {
                    if ($sample.owned_foreground) { $foregroundAcquired = $true }
                    if ($foregroundAcquired -and -not $sample.owned_foreground -and
                        $sample.elapsed_ms -lt $timer.Elapsed.TotalMilliseconds - 250) {
                        throw 'Owned game window was hidden/background during the process lifetime'
                    }
                }
                $console = ($logs | Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object { [IO.File]::ReadAllText($_) }) -join "`n"
                if ($console -notmatch '(?s)CSE_BENCH_START.*Host_EndGame:\s*End of demo\..*CSE_BENCH_DONE') {
                    throw 'Missing ordered start/full EOF/final markers'
                }
                $reports = [regex]::Matches($console, '(\d+) frames\s+([\d.]+) seconds\s+([\d.]+) fps')
                if ($reports.Count -ne 1 -or [int]$reports[0].Groups[1].Value -lt 650) { throw 'Missing/partial/ambiguous timedemo report' }
                $result.frames = [int]$reports[0].Groups[1].Value
                $result.seconds = [double]::Parse($reports[0].Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
                $result.fps = [double]::Parse($reports[0].Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
                $replay = $console.Substring($console.IndexOf('CSE_BENCH_START'))
                $replay = $replay.Substring(0, $replay.IndexOf('Host_EndGame:'))
                $profileCount = [regex]::Matches($replay, '(\d+) fps -- host\(\s*\d+\) sv\(\s*\d+\) cl\(\s*\d+\) gfx\(\s*\d+\) snd\(\s*\d+\) ents\(\d+\)').Count
                if ($profileCount -lt $result.frames - 1 -or $profileCount -gt $result.frames + 2) {
                    throw "Implausible profiler coverage: $profileCount profiles / $($result.frames) CLI frames"
                }
                $result.profile_count = $profileCount
                $result.success = $true
            } catch {
                $failure = $_.Exception.Message
            } finally {
                if ($process -and -not $process.HasExited) {
                    [void]$process.CloseMainWindow()
                    if (-not $process.WaitForExit(3000)) { $process.Kill(); [void]$process.WaitForExit(3000) }
                    $result.forced_cleanup = $true
                    if (-not $process.HasExited) { $failure = "$failure; owned PID cleanup failed" }
                }
                $result.wall_seconds = $timer.Elapsed.TotalSeconds
                if ($failure) { $result.failure = $failure; $result.success = $false }
                $index = 0
                foreach ($log in $logs) {
                    if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $EvidenceDirectory "$runPrefix-qconsole-$index.log") }
                    $index++
                }
                $result | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$runPrefix-result.json")
                $series.runs += "$runPrefix-result.json"
                $series | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $seriesPath
            }
            if ($failure) { throw $failure }
        }
    }
    foreach ($version in @('baseline', 'candidate')) {
        $sandbox = if ($version -eq 'baseline') { $BaselineSandboxDirectory } else { $CandidateSandboxDirectory }
        $after = Get-NativeManifest $sandbox
        if (($after | ConvertTo-Json -Depth 4 -Compress) -ne ($manifestBefore[$version] | ConvertTo-Json -Depth 4 -Compress)) {
            throw "Native providers changed during benchmark series: $version"
        }
    }
    $series.success = $true
} catch {
    $series.failure = $_.Exception.Message
    throw
} finally {
    $env:SteamAppId = $oldApp
    $env:SteamGameId = $oldGame
    $series.finished_utc = [DateTime]::UtcNow.ToString('o')
    $series | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $seriesPath
}
