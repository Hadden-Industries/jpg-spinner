<#
.SYNOPSIS
Checks the authored package identity, access contract and effective SDK setup.
.DESCRIPTION
Uses .NET XML and native MSBuild evaluation. It does not install a package, run
the app, validate a signature or prove runtime AppContainer access denial.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$GeneratedManifestPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PackageManifestPolicy.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$manifest = [Xml.XmlDocument]::new()
$manifest.Load((Join-Path $root 'src/JpgSpinner.App/Package.appxmanifest'))
$association = [Xml.XmlDocument]::new()
$association.Load((Join-Path $root 'src/JpgSpinner.App/Package.StoreAssociation.xml'))
Assert-JpgSpinnerPackageManifest -Manifest $manifest -StoreAssociation $association
if (-not [string]::IsNullOrWhiteSpace($GeneratedManifestPath)) {
    $generated = [Xml.XmlDocument]::new()
    $generated.Load([IO.Path]::GetFullPath($GeneratedManifestPath))
    Assert-JpgSpinnerPackageManifest -Manifest $generated -StoreAssociation $association
}
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
$evaluation = & $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
    -nologo -noAutoResponse '-p:Configuration=Debug' '-p:Platform=x64' `
    '-getProperty:WindowsAppSDKSelfContained,WindowsAppSdkBootstrapInitialize,WindowsAppSdkDeploymentManagerInitialize,WindowsAppSdkUndockedRegFreeWinRTInitialize,WindowsAppSdkAutoInitialize,DisableWindowsAppSDKMLAutoInitialize,WindowsAppSDKMLAutoInitialize,AppxAddDefaultTargetDeviceFamilyItem' 2>&1
if ($LASTEXITCODE -ne 0) { throw "Native project evaluation failed: $evaluation" }
$properties = (ConvertFrom-JsonWithUniqueObjectMembers -JsonText ($evaluation -join "`n") `
    -SourceDescription 'Packaged app native initialization properties' -RequiredRootValueKind Object).Properties
foreach ($name in @('WindowsAppSDKSelfContained', 'WindowsAppSdkBootstrapInitialize',
    'WindowsAppSdkDeploymentManagerInitialize', 'WindowsAppSdkUndockedRegFreeWinRTInitialize', 'WindowsAppSdkAutoInitialize',
    'AppxAddDefaultTargetDeviceFamilyItem')) {
    $isEnabled = $false
    if (-not [bool]::TryParse($properties.$name, [ref]$isEnabled) -or $isEnabled) {
        throw "$name must be explicitly false for the framework-dependent package."
    }
}
$isMlInitializationDisabled = $false
if (-not [bool]::TryParse($properties.DisableWindowsAppSDKMLAutoInitialize, [ref]$isMlInitializationDisabled) -or
    -not $isMlInitializationDisabled -or
    $properties.WindowsAppSDKMLAutoInitialize -ieq 'true') {
    throw 'The unused Windows ML auto-initializer must be disabled through DisableWindowsAppSDKMLAutoInitialize.'
}
Write-Output 'PASS: exact Store identity, empty capabilities, AppContainer declarations and native framework-dependent initialization policy.'
