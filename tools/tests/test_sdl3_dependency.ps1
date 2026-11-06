[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Resolve-Path "$PSScriptRoot\..\..").ProviderPath,
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [string]$MSBuildPath
)
$ErrorActionPreference = 'Stop'
if (Test-Path $EvidenceDirectory) { throw 'Use a fresh evidence directory.' }
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$EvidenceDirectory = (Resolve-Path $EvidenceDirectory).ProviderPath
$RepositoryRoot = (Resolve-Path $RepositoryRoot).ProviderPath
if (-not $MSBuildPath) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $installation = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
    $MSBuildPath = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
}
$sdk = Join-Path $EvidenceDirectory 'fixture\GoldSrc-SDK'
$payload = Join-Path $sdk 'external\SDL3'
$shared = Join-Path $sdk 'projects\vs2026'
New-Item -ItemType Directory -Path (Split-Path $payload), $shared | Out-Null
Copy-Item "$RepositoryRoot\GoldSrc-SDK\external\SDL3" $payload -Recurse
Copy-Item "$RepositoryRoot\GoldSrc-SDK\projects\vs2026\SDL3.props" $shared
Copy-Item "$RepositoryRoot\GoldSrc-SDK\projects\vs2026\SDL3.targets" $shared
$project = Join-Path $EvidenceDirectory 'fixture.proj'
@"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><CseSdl3Shared>true</CseSdl3Shared></PropertyGroup>
  <Import Project="$shared\SDL3.props" />
  <Target Name="Build" />
  <Import Project="$shared\SDL3.targets" />
</Project>
"@ | Set-Content -Encoding UTF8 $project
function Invoke-Check([string]$Name, [string]$Program, [string[]]$Arguments, [bool]$ExpectSuccess, [string]$ExpectedError = '') {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $Program @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $output | Set-Content (Join-Path $EvidenceDirectory "$Name.log")
    if (($code -eq 0) -ne $ExpectSuccess) { throw "$Name exit $code; see $Name.log" }
    if ($ExpectedError -and (($output | Out-String) -notmatch $ExpectedError)) { throw "$Name missed expected diagnostic $ExpectedError" }
    Write-Output "$Name PASS (exit $code)"
}
$msbuildArgs = @($project, '/nologo', '/v:minimal', '/p:Platform=Win32', '/p:Configuration=Release')
$module = "$RepositoryRoot\cmake\SDL3Dependency.cmake"
$cmakeArgs = @("-DCSE_SDK_DIR=$sdk", '-P', $module)
Invoke-Check 'cmake-validation' 'cmake' $cmakeArgs $true
Invoke-Check 'msbuild-stage' $MSBuildPath ($msbuildArgs + '/t:Build') $true
$staged = Join-Path $sdk 'bin\Win32\Release\SDL3.dll'
$expected = (Get-FileHash "$payload\lib\x86\SDL3.dll").Hash
if ((Get-FileHash $staged).Hash -ne $expected) { throw 'Incorrect staged SDL3 DLL.' }
Remove-Item $staged
Invoke-Check 'msbuild-restage' $MSBuildPath ($msbuildArgs + '/t:Build') $true
if ((Get-FileHash $staged).Hash -ne $expected) { throw 'Up-to-date MSBuild did not restore SDL3 DLL.' }
Invoke-Check 'msbuild-non-x86' $MSBuildPath @($project, '/nologo', '/v:minimal', '/t:CseValidateSdl3', '/p:Platform=x64', '/p:Configuration=Release') $false 'requires Windows x86'
# A real native x86 link validates upstream SHARED/IMPORTED_IMPLIB metadata.
$cmakeSource = Join-Path $EvidenceDirectory 'cmake-source'
New-Item -ItemType Directory $cmakeSource | Out-Null
'#include <SDL3/SDL.h>' + "`n" + '__declspec(dllexport) int probe(void) { return SDL_GetVersion(); }' | Set-Content "$cmakeSource\probe.c"
$moduleForward = $module.Replace('\', '/')
$sdkForward = $sdk.Replace('\', '/')
@"
cmake_minimum_required(VERSION 4.2)
project(Sdl3DependencyProbe LANGUAGES C)
set(CSE_SDK_DIR "$sdkForward")
include("$moduleForward")
get_target_property(provider_type SDL3::SDL3-shared TYPE)
if(NOT provider_type STREQUAL "SHARED_LIBRARY")
  message(FATAL_ERROR "SDL3 provider lost shared-library classification")
endif()
foreach(consumer probe client_probe)
  add_library(`${consumer} SHARED probe.c)
  cse_link_sdl3(`${consumer} Release)
  cse_link_sdl3(`${consumer} Debug)
  set_target_properties(`${consumer} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "`${CMAKE_BINARY_DIR}/runtime/`$<CONFIG>/valve/cl_dlls")
endforeach()
"@ | Set-Content "$cmakeSource\CMakeLists.txt"
$cmakeBuild = Join-Path $EvidenceDirectory 'cmake-build'
Invoke-Check 'cmake-configure-x86' 'cmake' @('-S', $cmakeSource, '-B', $cmakeBuild, '-G', 'Visual Studio 18 2026', '-A', 'Win32', '-T', 'v145', '-DCMAKE_SYSTEM_VERSION=10.0.26100.0') $true
foreach ($configuration in @('Release', 'Debug')) {
    $buildArgs = @('--build', $cmakeBuild, '--config', $configuration, '--target', 'probe', 'client_probe', '--parallel')
    Invoke-Check "cmake-build-$configuration" 'cmake' $buildArgs $true
    $cmakeStaged = Join-Path $cmakeBuild "runtime\$configuration\SDL3.dll"
    if ((Get-FileHash $cmakeStaged).Hash -ne $expected) { throw 'Incorrect CMake staged DLL.' }
    Remove-Item $cmakeStaged
    Invoke-Check "cmake-restage-$configuration" 'cmake' @('--build', $cmakeBuild, '--config', $configuration, '--target', 'client_probe') $true
    if ((Get-FileHash $cmakeStaged).Hash -ne $expected) { throw 'Up-to-date CMake did not restore SDL3 DLL.' }
}
foreach ($leaf in @('SDL3.lib', 'SDL3.dll')) {
    $path = Join-Path $payload "lib\x86\$leaf"
    $original = [System.IO.File]::ReadAllBytes($path)
    try {
        Remove-Item $path
        Invoke-Check "cmake-configure-missing-$leaf" 'cmake' @('-S', $cmakeSource, '-B', $cmakeBuild) $false 'Missing SDL3 payload'
        Invoke-Check "cmake-missing-$leaf" 'cmake' $cmakeArgs $false 'Missing SDL3 payload'
        Invoke-Check "msbuild-missing-$leaf" $MSBuildPath ($msbuildArgs + '/t:CseValidateSdl3') $false 'Missing SDL3 payload'
        [System.IO.File]::WriteAllBytes($path, $original + [byte]1)
        Invoke-Check "cmake-configure-mutated-$leaf" 'cmake' @('-S', $cmakeSource, '-B', $cmakeBuild) $false 'SDL3 hash mismatch'
        Invoke-Check "cmake-mutated-$leaf" 'cmake' $cmakeArgs $false 'SDL3 hash mismatch'
        Invoke-Check "msbuild-mutated-$leaf" $MSBuildPath ($msbuildArgs + '/t:CseValidateSdl3') $false 'SDL3 hash mismatch'
    } finally {
        [System.IO.File]::WriteAllBytes($path, $original)
    }
}
Write-Output 'SDL3 dependency checks PASS; fixture only, no runtime executed.'
