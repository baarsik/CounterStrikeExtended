<# Run on VM1's interactive desktop against an isolated, already audited music fixture. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('cstrike', 'valve')][string]$Game,
    [Parameter(Mandatory=$true)][string]$SandboxDirectory,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][string]$NAudioAssembly,
    [Parameter(Mandatory=$true)][string]$ModuleCollectorPath,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$TestCaseLabel = 'music',
    [ValidateRange(30, 180)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'evidence.ps1')
$SandboxDirectory = [IO.Path]::GetFullPath($SandboxDirectory)
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory)
if (-not (Test-Path (Join-Path $SandboxDirectory '.cse-music-fixture'))) {
    throw 'Music probes require an explicitly prepared, owned music fixture'
}
foreach ($name in @('Mss32.dll', 'mssmp3.asi', 'mssvoice.asi', 'binkawin.asi')) {
    if (@(Get-ChildItem $SandboxDirectory -Recurse -File -Filter $name).Count -ne 0) {
        throw "Retired Miles provider remains: $name"
    }
}
$prefix = "$Game-$TestCaseLabel"
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
Reserve-ProbeEvidence $EvidenceDirectory $prefix | Out-Null
Add-Type -Path $NAudioAssembly
if (-not ('CseMusicCapture' -as [type])) {
Add-Type -ReferencedAssemblies $NAudioAssembly -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Collections.Concurrent;
using System.Globalization;
using System.Threading;
using NAudio.Wave;
public sealed class CseMusicCapture : IDisposable {
    private readonly WasapiLoopbackCapture capture = new WasapiLoopbackCapture();
    private readonly WaveFileWriter writer;
    private readonly ManualResetEvent stopped = new ManualResetEvent(false);
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly ConcurrentQueue<string> spans = new ConcurrentQueue<string>();
    private long written;
    public string[] Spans { get { return spans.ToArray(); } }
    public CseMusicCapture(string path) {
        writer = new WaveFileWriter(path, capture.WaveFormat);
        capture.DataAvailable += (s, e) => {
            double timestamp = clock.Elapsed.TotalSeconds - e.BytesRecorded / (double)capture.WaveFormat.AverageBytesPerSecond;
            spans.Enqueue(written + "," + timestamp.ToString("R", CultureInfo.InvariantCulture) + "," + e.BytesRecorded);
            writer.Write(e.Buffer, 0, e.BytesRecorded);
            written += e.BytesRecorded;
        };
        capture.RecordingStopped += (s, e) => stopped.Set();
        capture.StartRecording();
    }
    public double Seconds { get { return clock.Elapsed.TotalSeconds; } }
    public void Dispose() {
        capture.StopRecording();
        if (!stopped.WaitOne(5000)) throw new Exception("Music capture did not stop");
        capture.Dispose();
        writer.Dispose();
        stopped.Dispose();
    }
}
'@
}
if (-not ('CseMusicWindow' -as [type])) {
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CseMusicWindow {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
}
'@
}
$gameDirectory = Join-Path $SandboxDirectory $Game
$testTrack = Join-Path $gameDirectory 'media\cse-music-test.mp3'
if (-not (Test-Path $testTrack)) { throw 'The one-second upstream sweep fixture is missing' }
$map = if ($Game -eq 'cstrike') { 'de_dust2' } else { 'crossfire' }
$commands = @('developer 1', 'sv_lan 1', 'fps_override 0', 'fps_max 100', 'gl_vsync 0', 'volume 0', "map $map")
$commands += @('wait') * 400
$commands += @('MP3Volume 1', 'mp3 loop media/cse-music-test.mp3', 'echo CSE_MUSIC_FULL')
$commands += @('wait') * 300
$commands += @('cd pause', 'echo CSE_MUSIC_PAUSE')
$commands += @('wait') * 200
$commands += @('cd resume', 'echo CSE_MUSIC_RESUME')
$commands += @('wait') * 200
$commands += @('MP3Volume 0.25', 'wait', 'cd mp3info', 'echo CSE_MUSIC_LOW')
$commands += @('wait') * 200
$commands += @('MP3FadeTime 0.5', 'cd fadeout', 'echo CSE_MUSIC_FADE')
$commands += @('wait') * 100
$commands += @('echo CSE_MUSIC_FADED', 'cd info')
$commands += @('wait') * 200
$commands += @('MP3Volume 0.6', 'mp3 play media/Half-Life01.mp3', 'echo CSE_MUSIC_STOCK')
$commands += @('wait') * 200
$commands += @('mp3 loopfile media/cse-music-test.mp3', 'echo CSE_MUSIC_REPLACED')
$commands += @('wait') * 10
$commands += @('cd mp3info')
$commands += @('wait') * 90
$commands += @('mp3 stop', 'echo CSE_MUSIC_STOP')
$commands += @('wait') * 100
$commands += @('echo CSE_MUSIC_DONE', 'quit')
$config = ($commands -join "`n") + "`n"
if ([Text.Encoding]::ASCII.GetByteCount($config) -ge 16384) { throw 'Music config exceeds engine command buffer' }
[IO.File]::WriteAllText((Join-Path $gameDirectory 'csemusic.cfg'), $config, [Text.Encoding]::ASCII)
$logs = @((Join-Path $SandboxDirectory 'qconsole.log'), (Join-Path $gameDirectory 'qconsole.log'))
foreach ($path in $logs) { if (Test-Path $path) { Remove-Item $path -Force } }
$oldApp = $env:SteamAppId
$oldGame = $env:SteamGameId
$env:SteamAppId = if ($Game -eq 'cstrike') { '10' } else { '70' }
$env:SteamGameId = $env:SteamAppId
$process = $null
$capture = $null
$markers = [ordered]@{}
$result = [ordered]@{ success=$false; game=$Game; testcase=$TestCaseLabel; sandbox=$SandboxDirectory; pid=$null }
try {
    $capture = New-Object CseMusicCapture((Join-Path $EvidenceDirectory "$prefix.wav"))
    $arguments = "-steam -game $Game -maxplayers 2 -gl -windowed -forceres -w 640 -h 480 -novid -dev -console -condebug -insecure -nomaster +ip 127.0.0.1 +exec csemusic.cfg"
    $process = Start-Process (Join-Path $SandboxDirectory 'hl.exe') -WorkingDirectory $SandboxDirectory -ArgumentList $arguments -PassThru
    $result.pid = $process.Id
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $collected = $false
    while (-not $process.WaitForExit(50)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Owned music probe timed out' }
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
            [CseMusicWindow]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
        }
        foreach ($log in $logs) {
            if (-not (Test-Path $log)) { continue }
            $text = [IO.File]::ReadAllText($log)
            foreach ($match in [regex]::Matches($text, '(?m)^CSE_MUSIC_([A-Z]+)\s*$')) {
                $phase = $match.Groups[1].Value
                if (-not $markers.Contains($phase)) { $markers[$phase] = $capture.Seconds }
            }
        }
        if ($markers.Contains('FULL') -and -not $collected) {
            & $ModuleCollectorPath $process.Id (Join-Path $EvidenceDirectory "$prefix-modules.txt")
            if ($LASTEXITCODE -ne 0) { throw 'Live music module collection failed' }
            $modules = @(Get-Content (Join-Path $EvidenceDirectory "$prefix-modules.txt"))
            $decoderModules = @($modules | Where-Object { [IO.Path]::GetFileName($_) -ieq 'mpg123.dll' })
            if ($decoderModules.Count -ne 1 -or -not $decoderModules[0].Equals((Join-Path $SandboxDirectory 'mpg123.dll'), [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Music did not load the source-built decoder from the fixture'
            }
            foreach ($module in $modules) {
                if ([IO.Path]::GetFileName($module) -imatch '^(mss32|mss64)\.dll$|^(mssmp3|mssvoice|binkawin)\.asi$') {
                    throw "A retired Miles module was loaded: $module"
                }
            }
            $result.decoder_sha256 = (Get-FileHash $decoderModules[0]).Hash.ToLowerInvariant()
            $collected = $true
        }
    }
    $result.exit_code = $process.ExitCode
    if ($process.ExitCode -ne 0) { throw "Music probe exited with $($process.ExitCode)" }
    $console = ($logs | Where-Object { Test-Path $_ } | ForEach-Object { [IO.File]::ReadAllText($_) }) -join "`n"
    foreach ($match in [regex]::Matches($console, '(?m)^CSE_MUSIC_([A-Z]+)\s*$')) {
        $phase = $match.Groups[1].Value
        if (-not $markers.Contains($phase)) { $markers[$phase] = $capture.Seconds }
    }
    foreach ($phase in @('FULL','PAUSE','RESUME','LOW','FADE','FADED','STOCK','REPLACED','STOP','DONE')) {
        if (-not $markers.Contains($phase)) { throw "Missing music phase: $phase" }
    }
    if (-not $collected) { throw 'No live music module snapshot' }
    [IO.File]::WriteAllText((Join-Path $EvidenceDirectory "$prefix-console.log"), $console)
    $result.success = $true
} finally {
    if ($process -and -not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(5000)) { $process.Kill(); $process.WaitForExit() }
    }
    if ($capture) {
        $capture.Dispose()
        [IO.File]::WriteAllLines((Join-Path $EvidenceDirectory "$prefix-audio-spans.csv"), $capture.Spans)
    }
    $env:SteamAppId = $oldApp
    $env:SteamGameId = $oldGame
    $result.phases = $markers
    $result | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$prefix-result.json")
}
