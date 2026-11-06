# Evaluation only: no targets, builds, tests, DLLs or providers are executed.
param([string]$OutputDirectory = "$PSScriptRoot/../build/msbuild-evaluation")
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$msbuild = (Get-Command MSBuild.exe -ErrorAction Stop).Source
$projects = Get-ChildItem "$root/GoldSrc-SDK" -Filter *.vcxproj -Recurse |
    Where-Object { $_.FullName -notmatch '[\\/]external[\\/]' }
foreach ($project in $projects) {
    foreach ($configuration in @('Debug', 'Release')) {
        $prefix = Join-Path $OutputDirectory "$($project.BaseName)-$configuration"
        $arguments = @($project.FullName, '-nologo', "-p:Configuration=$configuration", '-p:Platform=Win32',
            '-getProperty:ConfigurationType,TargetName,TargetExt,OutDir,IntDir,PlatformToolset,WindowsTargetPlatformVersion,LinkIncremental,CharacterSet,WholeProgramOptimization',
            '-getItem:ClCompile,ResourceCompile,Library,ProjectReference')
        $result = & $msbuild @arguments
        if ($LASTEXITCODE -ne 0) { throw "MSBuild evaluation failed: $project / $configuration" }
        # Validate the emitted JSON before saving it as evidence.
        $null = ($result -join "`n") | ConvertFrom-Json
        [System.IO.File]::WriteAllText("$prefix.json", ($result -join "`n"))
        & $msbuild $project.FullName -nologo "-p:Configuration=$configuration" '-p:Platform=Win32' "-preprocess:$prefix.xml"
        if ($LASTEXITCODE -ne 0) { throw "MSBuild preprocessing failed: $project / $configuration" }
    }
}
Write-Output "Evaluated $($projects.Count) retained/optional projects, both configurations. No build targets ran."
