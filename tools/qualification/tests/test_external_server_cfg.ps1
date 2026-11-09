<# Exercise the actual owned cfg writer under Windows PowerShell/.NET Framework. #>
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ProbeScript,
      [Parameter(Mandatory=$true)][string]$OwnedTestDirectory,
      [Parameter(Mandatory=$true)][string]$Receipt)
$ErrorActionPreference='Stop'
$tokens=$null
$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($ProbeScript,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Probe script syntax error'}
$functions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-Cfg'},$true))
if($functions.Count -ne 1){throw 'Missing unique actual Write-Cfg implementation'}
Invoke-Expression $functions[0].Extent.Text
if(Test-Path $OwnedTestDirectory){throw 'Cfg test requires fresh owned directory'}
New-Item -ItemType Directory $OwnedTestDirectory|Out-Null
$game=$OwnedTestDirectory
$path=Join-Path $game 'stage.cfg'
$checks=0
try {
    Write-Cfg 'stage.cfg' @('echo FIRST')
    if([IO.File]::ReadAllText($path) -cne "echo FIRST`n"){throw 'Initial cfg differs'}
    $checks++
    foreach($index in 0..99){
        $expected="echo REPLACEMENT_$index`n"
        Write-Cfg 'stage.cfg' @("echo REPLACEMENT_$index")
        if([IO.File]::ReadAllText($path) -cne $expected){throw 'Replacement bytes differ'}
        if((Test-Path ($path+'.previous')) -or (Test-Path ($path+'.next'))){throw 'Replacement left temporary files'}
        $checks++
    }
    $before=[IO.File]::ReadAllText($path)
    $rejected=$false
    try { Write-Cfg 'stage.cfg' @(('x'*16384)) } catch { $rejected=$_.Exception.Message -eq 'Cfg chunk exceeds engine command-buffer limit' }
    if(!$rejected -or [IO.File]::ReadAllText($path) -cne $before){throw 'Oversize cfg altered destination'}
    $checks++
    [ordered]@{success=$true;checks=$checks;actual_source_sha256=(Get-FileHash $ProbeScript -Algorithm SHA256).Hash.ToLowerInvariant();powershell=$PSVersionTable.PSVersion.ToString();runtime=[Environment]::Version.ToString()}|ConvertTo-Json|Set-Content -Encoding UTF8 $Receipt
} finally { Remove-Item $OwnedTestDirectory -Recurse -Force }
