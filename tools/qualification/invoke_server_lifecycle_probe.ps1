<# Owned native server/HLTV lifecycle. Existing servers are read-only.
   RetainedConsole permits the owned GUI launchers to allocate developer consoles.
   The ConPTY mode is qualified for console images; GUI launchers allocate separately.
   Local LAN/insecure fixture only; this does not qualify authentication.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
    [Parameter(Mandatory=$true)][string]$RuntimeIdentities,
    [Parameter(Mandatory=$true)][string]$GameDataDirectory,
    [Parameter(Mandatory=$true)][string]$SandboxDirectory,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][string]$ModuleCollectorPath,
    [Parameter(Mandatory=$true)][string]$PythonPath,
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [int]$ServerPort=27441,
    [int]$HltvPort=27442,
    [switch]$RetainedConsole,
    [switch]$DedicatedOnly
)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Set-StrictMode -Version Latest
$RuntimeDirectory=[IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\')
$GameDataDirectory=[IO.Path]::GetFullPath($GameDataDirectory).TrimEnd('\')
$SandboxDirectory=[IO.Path]::GetFullPath($SandboxDirectory).TrimEnd('\')
$EvidenceDirectory=[IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd('\')
if($ServerPort -ne 27441 -or $HltvPort -ne 27442){throw 'Only the two allocated owned fixture ports are permitted'}
foreach($source in @($RuntimeDirectory,$GameDataDirectory,$EvidenceDirectory)){
    if($SandboxDirectory.Equals($source,[StringComparison]::OrdinalIgnoreCase) -or
       $SandboxDirectory.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase) -or
       $source.StartsWith($SandboxDirectory+'\',[StringComparison]::OrdinalIgnoreCase)){
        throw 'Owned server sandbox must be separate from source/evidence directories'
    }
}
if(Test-Path $SandboxDirectory){throw 'Server lifecycle requires a fresh owned sandbox'}
if(Test-Path $EvidenceDirectory){throw 'Server lifecycle requires fresh evidence'}
New-Item -ItemType Directory $SandboxDirectory,$EvidenceDirectory|Out-Null
[IO.File]::WriteAllText((Join-Path $SandboxDirectory '.cse-owned-server-lifecycle'),'Owned loopback-only native server lifecycle fixture.')
function Save-Json([string]$Name,$Value){
    $Value|ConvertTo-Json -Depth 10|Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory ($Name+'.json'))
}
function Protected-Servers {
    return @(foreach($id in @(6272,11240)){
        $process=Get-CimInstance Win32_Process -Filter "ProcessId=$id"
        if(!$process){throw 'Protected existing fixture server is absent'}
        [ordered]@{pid=$process.ProcessId;path=$process.ExecutablePath;created=$process.CreationDate.ToUniversalTime().ToString('o')}
    })
}
$protected=Protected-Servers
Save-Json 'protected-servers-before' $protected
$failure=$null
$exit=1
$oldApp=$env:SteamAppId
$oldGame=$env:SteamGameId
try {
    $endpoints=@(Get-NetUDPEndpoint -LocalPort $ServerPort,$HltvPort -ErrorAction SilentlyContinue)
    Save-Json 'ports-before' @{ports=@($ServerPort,$HltvPort);endpoints=$endpoints}
    if($endpoints.Count){throw 'Allocated server/HLTV port is occupied'}
    $identities=Get-Content $RuntimeIdentities -Raw|ConvertFrom-Json
    if($identities.Count -ne 30){throw 'Expected sealed30-image runtime'}
    foreach($item in $identities){
        if((Get-FileHash (Join-Path $RuntimeDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256){throw ('Producing image changed: '+$item.relative)}
    }
    & robocopy.exe $GameDataDirectory $SandboxDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -ge 8){throw 'Game data staging failed'}
    & robocopy.exe $RuntimeDirectory $SandboxDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -ge 8){throw 'Native runtime overlay failed'}
    foreach($item in $identities){
        if((Get-FileHash (Join-Path $SandboxDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256){throw ('Sandbox image changed: '+$item.relative)}
    }
    Save-Json 'runtime-overlay' @($identities|Select-Object relative,bytes,sha256)
    $config=@('sv_lan 1','mp_consistency 1','hostname "CSE Native Lifecycle"','sv_password ""','mp_timelimit 0','developer 1','log on')
    [IO.File]::WriteAllLines((Join-Path $SandboxDirectory 'cstrike\server.cfg'),$config,[Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $SandboxDirectory 'cstrike\autoexec.cfg'),'// owned isolated fixture',[Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $SandboxDirectory 'hltv.cfg'),'// owned fixture: commands supplied explicitly on CLI',[Text.Encoding]::ASCII)
    $env:SteamAppId='10'
    $env:SteamGameId='10'
    $transportArguments=@()
    if($RetainedConsole){$transportArguments+=@('--retained-console')}
    if($DedicatedOnly){$transportArguments+=@('--dedicated-only')}
    & $PythonPath (Join-Path $PSScriptRoot 'server_lifecycle_probe.py') --sandbox $SandboxDirectory --evidence $EvidenceDirectory --identities $RuntimeIdentities --module-collector $ModuleCollectorPath --configuration $Configuration --server-port $ServerPort --hltv-port $HltvPort @transportArguments
    $exit=$LASTEXITCODE
    if($exit -ne 0){throw 'Owned native lifecycle failed; see result and console logs'}
} catch {
    $failure=$_.Exception.Message
} finally {
    $env:SteamAppId=$oldApp
    $env:SteamGameId=$oldGame
    try {
        $after=Protected-Servers
        Save-Json 'protected-servers-after' $after
        if(($protected|ConvertTo-Json -Depth 4 -Compress) -ne ($after|ConvertTo-Json -Depth 4 -Compress)){throw 'Protected server identity changed'}
        $owned=@(Get-CimInstance Win32_Process|Where-Object {$_.ExecutablePath -and $_.ExecutablePath.StartsWith($SandboxDirectory+'\',[StringComparison]::OrdinalIgnoreCase)}|Select-Object ProcessId,Name,ExecutablePath)
        $remaining=@(Get-NetUDPEndpoint -LocalPort $ServerPort,$HltvPort -ErrorAction SilentlyContinue)
        Save-Json 'cleanup' @{owned_processes=$owned;allocated_ports_remaining=@($remaining|Select-Object LocalAddress,LocalPort,OwningProcess)}
        if($owned.Count -or $remaining.Count){throw 'Owned process/port remains after lifecycle'}
    } catch {$failure="$failure; cleanup: $($_.Exception.Message)"}
    Save-Json 'wrapper-result' @{success=(!$failure -and $exit -eq 0);configuration=$Configuration;failure=$failure;helper_exit=$exit}
}
if($failure){throw $failure}
