<# Link the installed public Adobe XMP archive with native debug information
   and LINK warnings-as-errors. Missing private compiler PDBs are a real failure,
   not an ignored diagnostic. CMake and the selected MSVC toolset own parsing,
   package discovery, compilation and linking; this test invents none of them. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$InstalledDependencyPrefix
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) {
    throw 'Native debug-information qualification requires a fresh output directory.'
}
$toolchain = & (Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1')
$platform = @{x86 = 'Win32'; x64 = 'x64'; ARM64 = 'ARM64'}[$Architecture]
# The caller identifies the actual installed package prefix. Do not guess its
# hierarchy, query private integration properties, or restore a different graph
# merely to make this independent consumer link.
$installed = [IO.Path]::GetFullPath($InstalledDependencyPrefix)
if (-not (Test-Path -LiteralPath $installed -PathType Container)) {
    throw 'The selected installed native dependency triplet is absent.'
}
& $toolchain.cmakeExecutablePath -S (Join-Path $repositoryRoot 'eng/AdobeXmpDebugInformationProbe') `
    -B $output -G 'Visual Studio 18 2026' -A $platform `
    -T "v145,host=x64,version=$($toolchain.vcToolsVersion)" `
    '-DCMAKE_SYSTEM_VERSION=10.0.28000.0' "-DCMAKE_PREFIX_PATH=$installed"
if ($LASTEXITCODE -ne 0) { throw 'Native public-XMP probe configuration failed.' }
& $toolchain.cmakeExecutablePath --build $output --config $Configuration --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'The installed static XMP archive failed native debug-information linking.' }
# ARM64 cross-linking is useful evidence but cannot establish runtime behavior
# on this x64 host. Let native CTest run only the architectures this host runs.
if ($Architecture -ne 'ARM64') {
    $ctest = Join-Path (Split-Path -Parent $toolchain.cmakeExecutablePath) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
        throw 'The selected native CMake distribution lacks CTest.'
    }
    & $ctest --test-dir $output -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'The public XMP initialization probe failed.' }
}
Write-Output "PASS: $Architecture $Configuration installed public Adobe XMP links with /DEBUG and /WX."
