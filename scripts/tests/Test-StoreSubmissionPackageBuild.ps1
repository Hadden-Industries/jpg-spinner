<# Exercise the real build entry point and validate the native SDK-selected
   unsigned Store component. This does not sign, install or submit the app. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)
Set-StrictMode -Version Latest
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$working = [IO.Path]::GetFullPath($WorkingDirectory)
if (Test-Path -LiteralPath $working) { throw 'Store package test requires fresh owned work.' }
[void][IO.Directory]::CreateDirectory($working)
$packages = Join-Path $working packages
Import-Module (Join-Path $root scripts/ArtifactToolProcess.psm1) -Force
[void](Invoke-ArtifactToolProcess -Executable (Get-Process -Id $PID).Path -WorkingDirectory $RepositoryRoot `
    -LogDirectory $working -LogName build -TimeoutSeconds (Get-QualificationTimeout -Category nativeBuildProcess) -Arguments @('-NoProfile', '-File',
        (Join-Path $root scripts/Invoke-Build.ps1), '-Configuration', 'Release', '-Architecture', $Architecture,
        '-BuildEntryPoint', 'PackagedApplication', '-UnsignedPackageOutputDirectory', $packages,
        '-PrepareStoreSubmissionPackage', '-RepositoryRoot', $RepositoryRoot))
$selectionPath = Join-Path $packages store-submission-package-path.txt
if (-not [IO.File]::Exists($selectionPath)) { throw 'Native build did not report the Store submission component path.' }
# One native -getProperty request writes one scalar value, not a JSON object.
# The SDK chooses the filename; do not select the ordinary sibling package.
$selection = @(Get-Content -LiteralPath $selectionPath)
if ($selection.Count -ne 1 -or [string]::IsNullOrWhiteSpace($selection[0]) -or
    -not [IO.Path]::IsPathFullyQualified($selection[0])) { throw 'Native Store component selection is not one absolute path.' }
$selected = $selection[0]
$relative = [IO.Path]::GetRelativePath($packages, $selected)
if ([IO.Path]::IsPathRooted($relative) -or $relative -eq '..' -or $relative.StartsWith('..\', [StringComparison]::Ordinal)) {
    throw 'Native Store component escaped the owned package output.'
}
if (-not [IO.File]::Exists($selected) -or [IO.Path]::GetExtension($selected) -cne '.msix') {
    throw 'Native Store component is not an existing MSIX file.'
}
& (Join-Path $root scripts/tests/Test-UnsignedMsixPackage.ps1) -PackagePath $selected -Architecture $Architecture `
    -ExtractionDirectory (Join-Path $working inspection)
Write-Output "PASS: native $Architecture Store submission component selected and validated without signing/installing."
