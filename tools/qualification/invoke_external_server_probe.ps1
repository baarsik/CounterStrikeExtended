<# Owned native client against the unchanged loopback ReHLDS/Metamod/AMXX fixture.
Run in the normal interactive user session (ScheduledTask Interactive/Limited).
Never writes server files, reads server configuration, sends RCON, or weakens policy.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
    [Parameter(Mandatory=$true)][string]$RuntimeIdentities,
    [Parameter(Mandatory=$true)][string]$GameDataDirectory,
    [Parameter(Mandatory=$true)][string]$SandboxDirectory,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][string]$ServerDirectory,
    [Parameter(Mandatory=$true)][string]$ServerPins,
    [Parameter(Mandatory=$true)][string]$ServerProcessIdentity,
    [Parameter(Mandatory=$true)][string]$ServerCriticalInputs,
    [Parameter(Mandatory=$true)][string]$ModuleCollectorPath,
    [Parameter(Mandatory=$true)][string]$PythonPath,
    [ValidateRange(1,65535)][int]$ServerPort = 27438,
    [int]$ServerProcessId = 6272,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$TestCaseLabel = 'native-v9-Release-external-v1',
    [ValidateRange(30,300)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'evidence.ps1')
$analyzer = Join-Path $PSScriptRoot 'analyze_external_server_probe.py'
$RuntimeDirectory = [IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\')
$GameDataDirectory = [IO.Path]::GetFullPath($GameDataDirectory).TrimEnd('\')
$SandboxDirectory = [IO.Path]::GetFullPath($SandboxDirectory).TrimEnd('\')
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd('\')
$ServerDirectory = [IO.Path]::GetFullPath($ServerDirectory).TrimEnd('\')
function Contains-Path([string]$Parent, [string]$Child) {
    return $Child.Equals($Parent,[StringComparison]::OrdinalIgnoreCase) -or $Child.StartsWith($Parent+'\',[StringComparison]::OrdinalIgnoreCase)
}
foreach($path in @($RuntimeDirectory,$GameDataDirectory,$EvidenceDirectory,$ServerDirectory)) {
    if((Contains-Path $path $SandboxDirectory) -or (Contains-Path $SandboxDirectory $path)) { throw 'Sandbox must be separate from all source/server/evidence paths' }
}
$marker = Join-Path $SandboxDirectory '.cse-external-server-probe'
if((Test-Path $SandboxDirectory) -and !(Test-Path $marker) -and @(Get-ChildItem $SandboxDirectory -Force).Count) { throw 'Refusing nonempty unowned sandbox' }
if(@(Get-CimInstance Win32_Process -Filter "Name='hl.exe'").Count) { throw 'Another game process is active; desktop must be serialized' }
New-Item -ItemType Directory -Path $SandboxDirectory,$EvidenceDirectory -Force | Out-Null
[IO.File]::WriteAllText($marker,'Owned external-server compatibility sandbox.')
$prefix = 'cstrike-'+$TestCaseLabel
Reserve-ProbeEvidence $EvidenceDirectory $prefix | Out-Null
function Save-Json([string]$Name,$Value) {
    $Value | ConvertTo-Json -Depth 12 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory "$prefix-$Name.json")
}
function Verify-Server {
    $server = Get-CimInstance Win32_Process -Filter "ProcessId=$ServerProcessId"
    $identity = Get-Content $ServerProcessIdentity -Raw | ConvertFrom-Json
    $expectedPath = Join-Path $ServerDirectory 'hlds.exe'
    if(!$server -or $identity.pid -ne $ServerProcessId -or !$identity.path.Equals($expectedPath,[StringComparison]::OrdinalIgnoreCase) -or $server.CreationDate.ToUniversalTime().ToString('o') -ne $identity.created) { throw 'Pinned server process identity/start changed' }
    if($server.ExecutablePath -and !$server.ExecutablePath.Equals($expectedPath,[StringComparison]::OrdinalIgnoreCase)) { throw 'Pinned server executable changed' }
    $endpoint = @(Get-NetUDPEndpoint -LocalPort $ServerPort | Where-Object { $_.OwningProcess -eq $ServerProcessId -and $_.LocalAddress -eq '127.0.0.1' })
    if($endpoint.Count -ne 1) { throw 'Pinned loopback server endpoint changed' }
    $prepared = Get-Content $ServerPins -Raw | ConvertFrom-Json
    $wanted = @{}
    foreach($property in $prepared.native_hashes.PSObject.Properties) { $wanted[$property.Name.Replace('\','/')] = $property.Value }
    foreach($item in (Get-Content $ServerCriticalInputs -Raw | ConvertFrom-Json)) { $wanted[$item.relative.Replace('\','/')] = $item.sha256 }
    $rows = @(foreach($relative in @($wanted.Keys | Sort-Object)) {
        if($relative.Contains('..') -or [IO.Path]::IsPathRooted($relative)) { throw 'Invalid pinned server path' }
        $path = Join-Path $ServerDirectory $relative
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if($hash -ne $wanted[$relative].ToLowerInvariant()) { throw "Unchanged server input differs: $relative" }
        [ordered]@{relative=$relative;sha256=$hash;bytes=(Get-Item $path).Length}
    })
    return [ordered]@{pid=$ServerProcessId;created=$server.CreationDate.ToUniversalTime().ToString('o');address='127.0.0.1';port=$ServerPort;inputs=$rows}
}
$logDirectory = Join-Path $ServerDirectory 'cstrike\addons\amxmodx\logs'
$logOffsets = @{}
foreach($file in Get-ChildItem $logDirectory -Filter 'L*.log' -File) { $logOffsets[$file.FullName] = $file.Length }
function Read-FixtureDelta {
    $lines = @()
    foreach($file in Get-ChildItem $logDirectory -Filter 'L*.log' -File) {
        $offset = if($logOffsets.ContainsKey($file.FullName)) { [long]$logOffsets[$file.FullName] } else { 0L }
        if($file.Length -lt $offset) { throw 'Server log truncated during probe' }
        $stream = [IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        try {
            $stream.Position = $offset
            $reader = New-Object IO.StreamReader($stream)
            while(!$reader.EndOfStream) {
                $line = $reader.ReadLine()
                if($line -match '(CSE_FIXTURE_[A-Z_]+(?: [^\r\n]*)?)') { $lines += $Matches[1] }
            }
        } finally { $stream.Dispose() }
    }
    return $lines
}
$process = $null
$failure = $null
$oldApp = $env:SteamAppId
$oldGame = $env:SteamGameId
$result = [ordered]@{success=$false;scope='first secure native Release external-server probe';label=$TestCaseLabel;server_policy_changed=$false;voice_qualified=$false;audibility_qualified=$false;started_utc=[DateTime]::UtcNow.ToString('o')}
$game = Join-Path $SandboxDirectory 'cstrike'
$demo = Join-Path $game 'cse_external.dem'
$logs = @((Join-Path $SandboxDirectory 'qconsole.log'),(Join-Path $game 'qconsole.log'))
function Client-Console {
    $text = ''
    foreach($path in $logs) { if(Test-Path $path) { $text += Read-SharedProbeLog $path } }
    return $text
}
function Write-Cfg([string]$Name,[string[]]$Commands) {
    $text = ($Commands -join "`n")+"`n"
    if([Text.Encoding]::ASCII.GetByteCount($text) -ge 16384) { throw 'Cfg chunk exceeds engine command-buffer limit' }
    $path = Join-Path $game $Name
    $temporary = $path+'.next'
    $backup = $path+'.previous'
    [IO.File]::WriteAllText($temporary,$text,[Text.Encoding]::ASCII)
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    while($true) {
        try {
            if(Test-Path $path) {
                [IO.File]::Replace($temporary,$path,$backup)
                if([IO.File]::ReadAllText($path,[Text.Encoding]::ASCII) -cne $text) { throw 'Cfg replacement differs' }
                [IO.File]::Delete($backup)
            } else { [IO.File]::Move($temporary,$path) }
            break
        } catch [IO.IOException] { if([DateTime]::UtcNow -gt $deadline) { throw }; Start-Sleep -Milliseconds 10 }
    }
}
function Stage([string]$Name) { Write-Cfg 'cse_external_tick.cfg' @('cse_ext_'+$Name) }
function Await([string]$Name,[scriptblock]$Condition,[int]$Seconds=30) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while(!(& $Condition)) {
        if($process -and $process.HasExited) { throw "Client exited before $Name (code $($process.ExitCode))" }
        if([DateTime]::UtcNow -gt $deadline) { throw "Timed out awaiting $Name" }
        Start-Sleep -Milliseconds 100
    }
}
try {
    Save-Json 'server-before' (Verify-Server)
    Save-Json 'server-log-offsets' $logOffsets
    $queryPath = Join-Path $EvidenceDirectory "$prefix-server-info-before.json"
    & $PythonPath $analyzer --query-port $ServerPort --output $queryPath
    if($LASTEXITCODE -eq 0) {
        $info = Get-Content $queryPath -Raw | ConvertFrom-Json
        if($info.query_available) {
            if(!$info.vac_secure -or $info.folder -ne 'cstrike' -or $info.app_id -ne 10) { throw 'Expected secure unchanged CS server endpoint' }
            $result.a2s_info_secure=$true
        } else {
            $result.a2s_info_secure=$null
            $result.a2s_info_query=$info.error
        }
    } else {
        $result.a2s_info_secure=$null
        $result.a2s_info_query='unavailable; authentication and gameplay gates remain required'
    }
    $identities = Get-Content $RuntimeIdentities -Raw | ConvertFrom-Json
    if($identities.Count -ne 30) { throw 'Expected the sealed full 30-image native runtime' }
    foreach($item in $identities) {
        $path = Join-Path $RuntimeDirectory $item.relative
        if((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256) { throw "Producing runtime image changed: $($item.relative)" }
    }
    & robocopy.exe $GameDataDirectory $SandboxDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if($LASTEXITCODE -ge 8) { throw 'Owned game-data copy failed' }
    & robocopy.exe $RuntimeDirectory $SandboxDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if($LASTEXITCODE -ge 8) { throw 'Full native overlay failed' }
    foreach($item in $identities) {
        if((Get-FileHash (Join-Path $SandboxDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256) { throw 'Owned runtime overlay hash mismatch' }
    }
    Save-Json 'runtime-overlay' @($identities | Select-Object relative,sha256,bytes)
    & icacls.exe $SandboxDirectory /grant '*S-1-15-2-2:(OI)(CI)(RX)' | Out-Null
    if($LASTEXITCODE -ne 0) { throw 'Owned LPAC read ACL failed' }
    $removed = @()
    foreach($file in Get-ChildItem $SandboxDirectory -Recurse -File) {
        $normalized = $file.FullName.Replace('\','/')
        if($normalized.EndsWith('/sound/cse_fixture/probe.wav',[StringComparison]::OrdinalIgnoreCase) -or $normalized.EndsWith('/cse_fixture/probe.txt',[StringComparison]::OrdinalIgnoreCase)) {
            $removed += $file.FullName.Substring($SandboxDirectory.Length+1)
            Remove-Item -LiteralPath $file.FullName
        }
    }
    Save-Json 'download-precondition' @{forced_absent=@('sound/cse_fixture/probe.wav','cse_fixture/probe.txt');removed_owned_copies=$removed}
    foreach($path in @($logs)+@($demo)) { if(Test-Path $path) { Remove-Item -LiteralPath $path } }
    Get-ChildItem $game -Filter 'HalfLife*.tga' -File | Remove-Item -Force
    $commands = @('developer 1','cl_showmessages 1','fps_max 100','gl_vsync 0','cl_allowdownload 1','cl_download_ingame 1','cl_updaterate 37','name CSE_Ext_V9','alias cse_ext_noop ""',
      'alias cse_ext_ready "record cse_external"',
      'alias cse_ext_join "alias cse_ext_join cse_ext_noop; jointeam 2; joinclass 1; spec_pos"',
      'alias cse_ext_request "alias cse_ext_request cse_ext_noop; cse_fixture_start"',
      'alias cse_ext_menu "alias cse_ext_menu cse_ext_noop; menuselect 1"',
      'alias cse_ext_ack "alias cse_ext_ack cse_ext_noop; cse_fixture_ack; screenshot; status"',
      'alias cse_ext_move "alias cse_ext_move cse_ext_noop; +forward; +right"',
      'alias cse_ext_fire "alias cse_ext_fire cse_ext_noop; -forward; -right; +attack"',
      'alias cse_ext_finish "alias cse_ext_finish cse_ext_noop; -attack; stop; echo CSE_EXT_RECORDING_DONE; disconnect; quit"',
      'alias cse_ext_poll "exec cse_external_tick.cfg; wait; cse_ext_poll"',
      "connect 127.0.0.1:$ServerPort",'cse_ext_poll')
    Write-Cfg 'cse_external_boot.cfg' $commands
    Stage 'ready'
    $env:SteamAppId='10'; $env:SteamGameId='10'
    $arguments = '-steam -game cstrike -gl -windowed -forceres -w 640 -h 480 -novid -dev -console -condebug -nomaster +exec cse_external_boot.cfg'
    $result.arguments=$arguments
    $process = Start-Process (Join-Path $SandboxDirectory 'hl.exe') -WorkingDirectory $SandboxDirectory -ArgumentList $arguments -PassThru
    $handle = $process.Handle
    $result.pid=$process.Id
    Save-Json 'start' $result
    Await 'actual active-map recording' { (Test-Path $demo) -and (Get-Item $demo).Length -ge 544 -and (Client-Console).Contains('recording to cse_external.dem.') } $TimeoutSeconds
    $result.active_map_recording=$true
    $rawModules=Join-Path $EvidenceDirectory "$prefix-modules.txt"
    & $ModuleCollectorPath $process.Id $rawModules
    if($LASTEXITCODE -ne 0) { throw 'Native module collector failed' }
    $modules=@(foreach($path in Get-Content $rawModules -Encoding UTF8) { [ordered]@{path=[string]$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()} })
    Save-Json 'modules' $modules
    $critical=@('hl.exe','hw.dll','filesystem_stdio.dll','vgui.dll','vgui2.dll','tier0.dll','Core.dll','DemoPlayer.dll','SDL3.dll','mpg123.dll','steam_api64.dll','cstrike/cl_dlls/client.dll','valve/cl_dlls/GameUI.dll','valve/cl_dlls/particleman.dll')
    foreach($relative in $critical) {
        $expected=@($identities | Where-Object { $_.relative.Equals($relative,[StringComparison]::OrdinalIgnoreCase) })
        $full=Join-Path $SandboxDirectory $relative
        $observed=@($modules | Where-Object { $_.path.Equals($full,[StringComparison]::OrdinalIgnoreCase) })
        if($expected.Count -ne 1 -or $observed.Count -ne 1 -or $observed[0].sha256 -ne $expected[0].sha256) { throw "Live native module mismatch: $relative" }
    }
    if(@($modules | Where-Object { $_.path.EndsWith('\cstrike\dlls\mp.dll',[StringComparison]::OrdinalIgnoreCase) }).Count) { throw 'Remote client loaded an owned server DLL' }
    $result.matched_native_modules=$critical
    Save-Json 'active-map' $result
    Stage 'join'
    Await 'actual alive demo samples' { & $PythonPath $analyzer --demo $demo --live alive 2>$null; $LASTEXITCODE -eq 0 } 30
    $result.active_map_and_alive=$true
    Await 'Steam-authorized server callback' { (@(Read-FixtureDelta) -join "`n") -match 'CSE_FIXTURE_AUTH id=\d+ steam_authorized=1' } 30
    Stage 'request'
    Await 'received chat/custom bodies and unhooked custom skip' {
        & $PythonPath $analyzer --demo $demo --live messages 2>$null
        $LASTEXITCODE -eq 0 -and (Client-Console).Contains('UserMsg: No pfn CSEProbe')
    } 20
    Stage 'menu'
    Await 'actual AMXX menu reply' { (@(Read-FixtureDelta) -join "`n") -match 'CSE_FIXTURE_MENU_REPLY id=\d+ item=0' } 15
    Stage 'ack'
    Await 'received ack TextMsg body and cvar reply' {
        & $PythonPath $analyzer --demo $demo --live ack 2>$null
        $LASTEXITCODE -eq 0 -and ((@(Read-FixtureDelta) -join "`n") -match 'CSE_FIXTURE_CVAR_REPLY id=\d+ name=cl_updaterate value=37(?:\.0*)?(?:\s|$)')
    } 15
    Stage 'move'
    Await 'actual alive movement and continuing network frames' { & $PythonPath $analyzer --demo $demo --live movement 2>$null; $LASTEXITCODE -eq 0 } 20
    Stage 'fire'
    Start-Sleep -Milliseconds 700
    Stage 'finish'
    if(!$process.WaitForExit(15000)) { throw 'Owned client did not exit after finalized recording' }
    $result.exit_code=$process.ExitCode
    if($process.ExitCode -ne 0) { throw "Owned client exited nonzero: $($process.ExitCode)" }
    $console=Client-Console
    $consolePath=Join-Path $EvidenceDirectory "$prefix-client-console.log"
    [IO.File]::WriteAllText($consolePath,$console)
    Copy-Item $demo (Join-Path $EvidenceDirectory "$prefix.dem")
    $delta=@(Read-FixtureDelta)
    $auth=@($delta | Where-Object { $_ -match '^CSE_FIXTURE_AUTH id=\d+ steam_authorized=1$' })
    if($auth.Count -ne 1) { throw 'Expected one correlated new authenticated fixture client' }
    $playerId=[int]([regex]::Match($auth[0],'id=(\d+)').Groups[1].Value)
    foreach($name in @('STARTED','MENU_REPLY','CVAR_REPLY','CLIENT_ACK')) {
        if(!(@($delta | Where-Object { $_ -match "^CSE_FIXTURE_$name id=$playerId(?: |$)" }).Count)) { throw "Missing correlated server callback: $name" }
    }
    $downloads=@()
    foreach($suffix in @('sound/cse_fixture/probe.wav','cse_fixture/probe.txt')) {
        $wanted=@((Get-Content $ServerCriticalInputs -Raw | ConvertFrom-Json) | Where-Object { $_.relative.Replace('\','/') -eq 'cstrike/'+$suffix })[0]
        $received=@(Get-ChildItem $SandboxDirectory -Recurse -File | Where-Object { $_.FullName.Replace('\','/').EndsWith('/'+$suffix,[StringComparison]::OrdinalIgnoreCase) })
        if(!$received.Count) { throw "Missing actual downloaded resource: $suffix" }
        foreach($file in $received) {
            $hash=(Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if($hash -ne $wanted.sha256) { throw "Received resource hash differs: $suffix" }
            $downloads += [ordered]@{relative=$file.FullName.Substring($SandboxDirectory.Length+1);sha256=$hash;bytes=$file.Length}
        }
    }
    Save-Json 'downloads' $downloads
    $analysisPath=Join-Path $EvidenceDirectory "$prefix-demo-analysis.json"
    & $PythonPath $analyzer --demo $demo --console $consolePath --player-id $playerId --output $analysisPath
    if($LASTEXITCODE -ne 0) { throw 'Received demo/message evidence is incomplete' }
    $result.success=$true
} catch { $failure=$_.Exception.Message } finally {
    if($process -and !$process.HasExited) {
        [void]$process.CloseMainWindow()
        $result.cleanup_close_requested=$true
        if(!$process.WaitForExit(3000)) { $process.Kill(); $process.WaitForExit(); $result.forced_cleanup=$true }
    }
    if($process -and $process.HasExited) { $result.exit_code=$process.ExitCode }
    $env:SteamAppId=$oldApp; $env:SteamGameId=$oldGame
    try {
        $delta=@(Read-FixtureDelta)
        [IO.File]::WriteAllLines((Join-Path $EvidenceDirectory "$prefix-server-fixture-delta.log"),$delta)
        Save-Json 'server-after' (Verify-Server)
        foreach($path in $logs) { if(Test-Path $path) { Copy-Item $path (Join-Path $EvidenceDirectory "$prefix-$([IO.Path]::GetFileName([IO.Path]::GetDirectoryName($path)))-qconsole.log") } }
        if(Test-Path $demo) { Copy-Item $demo (Join-Path $EvidenceDirectory "$prefix.dem") -Force }
        if(Test-Path $game) { Get-ChildItem $game -Filter 'HalfLife*.tga' -File | ForEach-Object { Copy-Item $_.FullName (Join-Path $EvidenceDirectory "$prefix-$($_.Name)") } }
    } catch { $failure="$failure; evidence/unchanged-server verification: $($_.Exception.Message)" }
    if($failure) { $result.success=$false; $result.failure=$failure }
    $result.finished_utc=[DateTime]::UtcNow.ToString('o')
    Save-Json 'result' $result
    $result | ConvertTo-Json -Depth 8
}
if($failure) { throw $failure }
