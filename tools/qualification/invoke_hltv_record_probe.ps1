<# Native HLTV broadcast producer or maintained-reader fixture; owned paths only.
   No protected server writes, RCON, public endpoints or security policy changes. #>
[CmdletBinding()]
param(
    [ValidateSet('producer','reader')][string]$Mode='producer',
    [ValidateSet('x86','x64')][string]$Architecture='x64',
    [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
    [Parameter(Mandatory=$true)][string]$RuntimeIdentities,
    [Parameter(Mandatory=$true)][string]$GameDataDirectory,
    [Parameter(Mandatory=$true)][string]$ServerDirectory,
    [Parameter(Mandatory=$true)][string]$FixturePins,
    [Parameter(Mandatory=$true)][string]$CriticalInputs,
    [Parameter(Mandatory=$true)][string]$ProtectedIdentities,
    [Parameter(Mandatory=$true)][string]$ClientDirectory,
    [string]$CloneDirectory,
    [string]$HltvDirectory,
    [string]$InputDemo,
    [string]$InputSha256,
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [Parameter(Mandatory=$true)][string]$ModuleCollector,
    [Parameter(Mandatory=$true)][string]$PythonPath,
    [Parameter(Mandatory=$true)][string]$DecoderDirectory,
    [Parameter(Mandatory=$true)][string]$ConsoleDirectory,
    [Parameter(Mandatory=$true)][string]$ReconnectDirectory,
    [ValidateSet('Release','Debug')][string]$Configuration
)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Set-StrictMode -Version Latest
foreach($name in @('RuntimeDirectory','GameDataDirectory','ServerDirectory','ClientDirectory','CloneDirectory','HltvDirectory','EvidenceDirectory')){
    if((Get-Variable $name).Value){Set-Variable -Name $name -Value ([IO.Path]::GetFullPath((Get-Variable $name).Value).TrimEnd('\'))}
}
if($Mode -eq 'producer' -and (!$CloneDirectory -or !$HltvDirectory -or $Architecture -ne 'x64')){throw 'Producer needs native HLTV and fresh clone paths'}
if($Mode -eq 'reader' -and (!(Test-Path $InputDemo -PathType Leaf) -or $InputSha256 -notmatch '^[0-9a-f]{64}$')){throw 'Reader needs sealed input path and producing SHA256'}
$ownedDirectories=@($ClientDirectory,$EvidenceDirectory)
if($Mode -eq 'producer'){$ownedDirectories+=@($CloneDirectory,$HltvDirectory)}
foreach($owned in $ownedDirectories){
    if(Test-Path $owned){throw 'HLTV fixture requires fresh owned directories'}
    foreach($source in @($RuntimeDirectory,$GameDataDirectory,$ServerDirectory)){
        if($source.Equals($owned,[StringComparison]::OrdinalIgnoreCase) -or
           $source.StartsWith($owned+'\',[StringComparison]::OrdinalIgnoreCase) -or
           $owned.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Own fixture overlaps source'}
    }
}
New-Item -ItemType Directory $ownedDirectories|Out-Null
[IO.File]::WriteAllText((Join-Path $ClientDirectory '.cse-owned-hltv-client'),'Owned HLTV player/reader fixture.')
if($Mode -eq 'producer'){
    [IO.File]::WriteAllText((Join-Path $CloneDirectory '.cse-owned-reconnect'),'Owned pinned Win32 server clone.')
    [IO.File]::WriteAllText((Join-Path $HltvDirectory '.cse-owned-hltv'),'Owned native HLTV producer.')
}
function Save-Json([string]$Name,$Value){$Value|ConvertTo-Json -Depth 12|Set-Content -Encoding UTF8 (Join-Path $EvidenceDirectory ($Name+'.json'))}
function Protected {
    $expected=Get-Content $ProtectedIdentities -Raw|ConvertFrom-Json
    return @(foreach($item in $expected){
        $p=Get-CimInstance Win32_Process -Filter "ProcessId=$($item.pid)"
        if(!$p -or $p.CreationDate.ToUniversalTime().ToString('o') -ne $item.created){throw 'Protected server changed'}
        if($p.ExecutablePath -and !$p.ExecutablePath.Equals($item.path,[StringComparison]::OrdinalIgnoreCase)){throw 'Protected server path changed'}
        [ordered]@{pid=$item.pid;created=$item.created;expected_path=$item.path}
    })
}
$wanted=@{}
$prepared=Get-Content $FixturePins -Raw|ConvertFrom-Json
foreach($property in $prepared.native_hashes.PSObject.Properties){$wanted[$property.Name.Replace('\','/')]=$property.Value}
foreach($item in (Get-Content $CriticalInputs -Raw|ConvertFrom-Json)){$wanted[$item.relative.Replace('\','/')]=$item.sha256}
function Verify-Server([string]$Base){
    return @(foreach($relative in @($wanted.Keys|Sort-Object)){
        $path=Join-Path $Base $relative
        $hash=(Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if($hash -ne $wanted[$relative].ToLowerInvariant()){throw ('Pinned Win32 server input changed: '+$relative)}
        [ordered]@{relative=$relative;bytes=(Get-Item $path).Length;sha256=$hash}
    })
}
function Config-Hashes([string]$Base){
    return @(Get-ChildItem $Base -Recurse -File|Where-Object {$_.Extension -in @('.cfg','.ini') -or $_.Name -eq 'liblist.gam'}|ForEach-Object {
        [ordered]@{relative=$_.FullName.Substring($Base.Length+1);sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    }|Sort-Object {$_.relative})
}
$failure=$null
$exit=1
$oldApp=$env:SteamAppId
$oldGame=$env:SteamGameId
$before=Protected
$serverBefore=Verify-Server $ServerDirectory
$configBefore=Config-Hashes $ServerDirectory
Save-Json 'protected-before' $before
Save-Json 'protected-server-before' $serverBefore
try {
    $ports=@(Get-NetUDPEndpoint -LocalPort 27443,27444,27445 -ErrorAction SilentlyContinue)
    Save-Json 'ports-before' @{endpoints=@($ports|Select-Object LocalAddress,LocalPort,OwningProcess)}
    if($ports.Count){throw 'Allocated clone ports are occupied'}
    if(@(Get-CimInstance Win32_Process -Filter "Name='hl.exe'").Count){throw 'Another desktop game is active'}
    $identities=Get-Content $RuntimeIdentities -Raw|ConvertFrom-Json
    if($Architecture -eq 'x86'){$identities=@($identities|Where-Object {$_.configuration -eq $Configuration})}
    $requiredCount=if($Architecture -eq 'x64'){30}else{21}
    if($identities.Count -ne $requiredCount){throw 'Wrong sealed runtime image count'}
    foreach($item in $identities){
        if((Get-FileHash (Join-Path $RuntimeDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256){throw 'Native producing image changed'}
    }
    if($Mode -eq 'producer'){
        & robocopy.exe $ServerDirectory $CloneDirectory /E /R:1 /W:1 /XF *.log *.dem server-pid.txt server-command.json /XD logs /NFL /NDL /NJH /NJS /NP|Out-Null
        if($LASTEXITCODE -ge 8){throw 'Pinned server clone copy failed'}
        $clonePins=Verify-Server $CloneDirectory
        $cloneConfig=Config-Hashes $CloneDirectory
        if(($configBefore|ConvertTo-Json -Compress) -ne ($cloneConfig|ConvertTo-Json -Compress)){throw 'Clone config differs from unchanged source'}
        Save-Json 'clone-config-hashes' $cloneConfig
        Save-Json 'clone-server-pins' $clonePins
        & robocopy.exe $GameDataDirectory $HltvDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
        if($LASTEXITCODE -ge 8){throw 'HLTV data staging failed'}
        & robocopy.exe $RuntimeDirectory $HltvDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
        if($LASTEXITCODE -ge 8){throw 'HLTV native overlay failed'}
        foreach($item in $identities){
            if((Get-FileHash (Join-Path $HltvDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256){throw 'HLTV staged image changed'}
        }
        [IO.File]::WriteAllText((Join-Path $HltvDirectory 'hltv.cfg'),'// Owned loopback HLTV; all commands supplied explicitly.',[Text.Encoding]::ASCII)
        Save-Json 'hltv-controlled-config' @{owned_hltv_cfg_empty=$true;protected_server_config_changed=$false}
    }
    & robocopy.exe $GameDataDirectory $ClientDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -ge 8){throw 'Client data staging failed'}
    & robocopy.exe $RuntimeDirectory $ClientDirectory /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -ge 8){throw 'Client native overlay failed'}
    foreach($item in $identities){
        if((Get-FileHash (Join-Path $ClientDirectory $item.relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256){throw 'Native client staged image changed'}
    }
    Save-Json 'native-overlay' @($identities|Select-Object relative,sha256,bytes)
    & icacls.exe $ClientDirectory /grant '*S-1-15-2-2:(OI)(CI)(RX)'|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Owned client LPAC read ACL failed'}
    if($Mode -eq 'producer'){
        $removed=@()
        foreach($file in Get-ChildItem $ClientDirectory -Recurse -File){
            $name=$file.FullName.Replace('\','/')
            if($name.EndsWith('/sound/cse_fixture/probe.wav') -or $name.EndsWith('/cse_fixture/probe.txt')){
                $removed+=$file.FullName.Substring($ClientDirectory.Length+1)
                Remove-Item $file.FullName
            }
        }
        Save-Json 'download-precondition' @{forced_absent=@('sound/cse_fixture/probe.wav','cse_fixture/probe.txt');removed_owned_copies=$removed}
        foreach($name in @('qconsole.log','cstrike\qconsole.log')){Remove-Item (Join-Path $ClientDirectory $name) -ErrorAction SilentlyContinue}
    }else{
        Copy-Item $InputDemo (Join-Path $ClientDirectory 'cstrike\cse_hltv_broadcast.dem')
        $inputHash=$InputSha256
        if((Get-FileHash $InputDemo -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputHash){throw 'Producing broadcast bytes changed'}
        if((Get-FileHash (Join-Path $ClientDirectory 'cstrike\cse_hltv_broadcast.dem') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputHash){throw 'Reader input copy differs'}
        Save-Json 'reader-input' @{path=$InputDemo;sha256=$inputHash}
        foreach($name in @('qconsole.log','cstrike\qconsole.log')){Remove-Item (Join-Path $ClientDirectory $name) -ErrorAction SilentlyContinue}
    }
    $env:SteamAppId='10'
    $env:SteamGameId='10'
    $modeArguments=@()
    if($Mode -eq 'producer'){$modeArguments+=@('--server',$CloneDirectory,'--hltv',$HltvDirectory,'--server-pins',(Join-Path $EvidenceDirectory 'clone-server-pins.json'))}
    else{$modeArguments+=@('--input-demo',$InputDemo,'--input-sha256',$inputHash)}
    & $PythonPath (Join-Path $PSScriptRoot 'hltv_record_probe.py') --mode $Mode --architecture $Architecture --client $ClientDirectory --evidence $EvidenceDirectory --identities $RuntimeIdentities --module-collector $ModuleCollector --decoder-directory $DecoderDirectory --console-directory $ConsoleDirectory --reconnect-directory $ReconnectDirectory --configuration $Configuration @modeArguments
    $exit=$LASTEXITCODE
    if($exit -ne 0){throw 'Actual HLTV producer/reader failed; see preserved result and logs'}
} catch {$failure=$_.Exception.Message} finally {
    $env:SteamAppId=$oldApp
    $env:SteamGameId=$oldGame
    try {
        $after=Protected
        $serverAfter=Verify-Server $ServerDirectory
        if($Mode -eq 'producer'){$cloneAfter=Verify-Server $CloneDirectory}
        Save-Json 'protected-after' $after
        Save-Json 'protected-server-after' $serverAfter
        if($Mode -eq 'producer'){Save-Json 'clone-server-after' $cloneAfter}
        if(($configBefore|ConvertTo-Json -Compress) -ne ((Config-Hashes $ServerDirectory)|ConvertTo-Json -Compress)){throw 'Protected configs changed'}
        if($Mode -eq 'producer' -and ($configBefore|ConvertTo-Json -Compress) -ne ((Config-Hashes $CloneDirectory)|ConvertTo-Json -Compress)){throw 'Clone configs changed'}
        $owned=@(Get-CimInstance Win32_Process|Where-Object {$_.ExecutablePath -and ($_.ExecutablePath.StartsWith($ClientDirectory+'\',[StringComparison]::OrdinalIgnoreCase) -or ($CloneDirectory -and $_.ExecutablePath.StartsWith($CloneDirectory+'\',[StringComparison]::OrdinalIgnoreCase)) -or ($HltvDirectory -and $_.ExecutablePath.StartsWith($HltvDirectory+'\',[StringComparison]::OrdinalIgnoreCase)))}|Select-Object ProcessId,Name,ExecutablePath)
        $remaining=@(Get-NetUDPEndpoint -LocalPort 27443,27444,27445 -ErrorAction SilentlyContinue|Select-Object LocalAddress,LocalPort,OwningProcess)
        Save-Json 'cleanup' @{owned_processes=$owned;allocated_ports_remaining=$remaining}
        if($owned.Count -or $remaining.Count){throw 'Own HLTV process/port remains'}
    } catch {$failure="$failure; cleanup: $($_.Exception.Message)"}
    Save-Json 'wrapper-result' @{success=(!$failure -and $exit -eq 0);configuration=$Configuration;failure=$failure;helper_exit=$exit}
}
if($failure){throw $failure}
