<# Use the maintained public API cases against either prepared source or the
   actual installed production package. Source qualification additionally runs
   one native clang-tidy type rule; no CodeQL/security scan is launched. #>
[CmdletBinding(DefaultParameterSetName = 'InstalledPackage')]
param(
    [Parameter(Mandatory, ParameterSetName = 'SourceQualification')][string]$SourceDirectory,
    [Parameter(Mandatory, ParameterSetName = 'InstalledPackage')][string]$InstalledDependencyPrefix,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = [IO.Path]::GetFullPath($EvidenceDirectory)
$build = [IO.Path]::GetFullPath($WorkingDirectory)
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $build)) {
    throw 'The Exiv2 charset qualification requires fresh working and evidence directories.'
}
$toolchain = & (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1')
Import-Module (Join-Path $root 'scripts/QualificationPolicy.psm1') -Force
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1') -Force
$platform = @{x86 = 'Win32'; x64 = 'x64'; ARM64 = 'ARM64' }[$Architecture]
[void][IO.Directory]::CreateDirectory($output)
$context = @{Executable = $toolchain.cmakeExecutablePath; WorkingDirectory = $root; LogDirectory = $output }
$arguments = @('-S', (Join-Path $root 'eng/Exiv2WindowsCharsetProbe'), '-B', $build,
    '-G', 'Visual Studio 18 2026', '-A', $platform,
    '-T', "v145,host=x64,version=$($toolchain.vcToolsVersion)", '-DCMAKE_SYSTEM_VERSION=10.0.28000.0')
if ($PSCmdlet.ParameterSetName -ceq 'InstalledPackage') {
    $installed = [IO.Path]::GetFullPath($InstalledDependencyPrefix)
    if (-not [IO.Directory]::Exists($installed)) { throw 'The actual installed dependency prefix is absent.' }
    $arguments += "-DEXIV2_INSTALLED_PREFIX=$installed"
}
else {
    $source = [IO.Path]::GetFullPath($SourceDirectory)
    if (-not [IO.File]::Exists((Join-Path $source 'src/convert.cpp'))) { throw 'The prepared Exiv2 source is absent.' }
    $clangTidy = Join-Path $toolchain.visualStudioInstallationPath 'VC/Tools/Llvm/x64/bin/clang-tidy.exe'
    $arguments += @("-DEXIV2_SOURCE_DIRECTORY=$source", "-DCLANG_TIDY_EXECUTABLE=$clangTidy")
}
if ($Architecture -eq 'ARM64') {
    # CMake owns its cross-compilation branch; the x64 host cannot run ARM64.
    $arguments += '-DCMAKE_SYSTEM_NAME=Windows'
}
[void](Invoke-ArtifactToolProcess @context -LogName configure -Arguments $arguments)
if ($PSCmdlet.ParameterSetName -ceq 'SourceQualification') {
    [void](Invoke-ArtifactToolProcess @context -LogName type-contract -TimeoutSeconds (Get-QualificationTimeout -Category compilerProbe) `
            -Arguments @('--build', $build, '--config', $Configuration, '--target', 'Exiv2WindowsCharsetTypes', '--parallel', '2'))
}
[void](Invoke-ArtifactToolProcess @context -LogName build -TimeoutSeconds (Get-QualificationTimeout -Category nativeBuildProcess) `
        -Arguments @('--build', $build, '--config', $Configuration, '--target', 'Exiv2WindowsCharsetProbe', '--parallel', '2'))
if ($Architecture -ne 'ARM64') {
    $ctest = Join-Path (Split-Path -Parent $toolchain.cmakeExecutablePath) 'ctest.exe'
    $context.Executable = $ctest
    [void](Invoke-ArtifactToolProcess @context -LogName compatibility -Arguments @('--test-dir', $build,
            '-C', $Configuration, '-R', '^WindowsCharsetCompatibility$', '--output-on-failure',
            '--output-junit', (Join-Path $output 'charset.junit.xml')))
    Write-Output "PASS: $Architecture $Configuration native Exiv2 charset compatibility."
}
else {
    Write-Output 'PASS: ARM64 Exiv2 charset client cross-compiles/links; native execution remains separate.'
}
