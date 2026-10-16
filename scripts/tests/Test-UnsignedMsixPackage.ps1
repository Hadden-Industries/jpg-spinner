<# Qualifies the actual unsigned application package with the owning SDK reader.
   It neither signs/installs a package nor claims runtime/Store acceptance. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$ExtractionDirectory,
    [Parameter(Mandatory)][ValidateSet('x86','x64','ARM64')][string]$Architecture
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$package = [IO.Path]::GetFullPath($PackagePath)
$extraction = [IO.Path]::GetFullPath($ExtractionDirectory)
if (-not (Test-Path -LiteralPath $package -PathType Leaf)) { throw 'The exact unsigned MSIX package is absent.' }
if (Test-Path -LiteralPath $extraction) { throw 'Native extraction requires a fresh owned output directory.' }
$toolchain = & (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
$platform = if ($Architecture -eq 'x86') { 'Win32' } else { $Architecture }
# Query the owning SDK's selected executable, not a second SDK/PATH fallback.
$makeAppxPath = & $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
    -nologo -noAutoResponse "-p:Platform=$platform" -p:GenerateAppxPackageOnBuild=true `
    '-getProperty:MakeAppxExeFullPath'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $makeAppxPath -PathType Leaf)) {
    throw 'The owning MSIX targets did not select a native MakeAppx executable.'
}
& $makeAppxPath unpack /p $package /d $extraction /no
if ($LASTEXITCODE -ne 0) { throw 'Native MakeAppx refused the unsigned application package.' }
Import-Module (Join-Path $root 'scripts/PackageManifestPolicy.psm1') -Force
$manifest = [Xml.XmlDocument]::new()
$manifest.Load((Join-Path $extraction 'AppxManifest.xml'))
$association = [Xml.XmlDocument]::new()
$association.Load((Join-Path $root 'src/JpgSpinner.App/Package.StoreAssociation.xml'))
Assert-JpgSpinnerPackageManifest -Manifest $manifest -StoreAssociation $association
$namespaces = [Xml.XmlNamespaceManager]::new($manifest.NameTable)
$namespaces.AddNamespace('p','http://schemas.microsoft.com/appx/manifest/foundation/windows10')
$identity = $manifest.SelectSingleNode('/p:Package/p:Identity',$namespaces)
if ($identity.GetAttribute('ProcessorArchitecture') -cne $Architecture.ToLowerInvariant()) {
    throw 'Actual package architecture differs from the requested native build.'
}
$framework = $manifest.SelectNodes('/p:Package/p:Dependencies/p:PackageDependency[@Name="Microsoft.WindowsAppRuntime.2"]', $namespaces)
if ($framework.Count -ne 1 -or [version]$framework.Item(0).GetAttribute('MinVersion') -ne [version]'2.5.1.0') {
    throw 'Actual package must retain its qualified native Windows App Runtime framework dependency.'
}
if (Test-Path -LiteralPath (Join-Path $extraction 'AppxSignature.p7x')) {
    throw 'Unsigned package qualification cannot accept a signed input.'
}
foreach ($required in @('JpgSpinner.exe','JpgSpinner.winmd','resources.pri','AppxBlockMap.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $extraction $required) -PathType Leaf)) {
        throw "Actual application package lacks required payload: $required"
    }
}
foreach ($file in Get-ChildItem -LiteralPath $extraction -File -Recurse) {
    if ($file.Name -like '*Tests*' -or $file.Name -like '*TestSupport*' -or $file.Name -eq 'Microsoft.UI.Xaml.dll' -or
        $file.Name -eq 'Microsoft.WindowsAppRuntime.Bootstrap.dll') {
        throw "Actual application package contains test support or private runtime: $($file.Name)"
    }
}
& (Join-Path $PSScriptRoot 'Test-PackageArtwork.ps1') -AssetsDirectory (Join-Path $extraction 'Assets')
# Verify the bytes the SDK actually packaged; an evaluated project item alone
# does not establish that its linked target survives native packaging.
& (Join-Path $PSScriptRoot 'Test-PackageNoticeContent.ps1') -PackageDirectory $extraction
Write-Output "PASS: native $Architecture MSIX reader, exact Store/AppContainer manifest, framework, unsigned payload, artwork and retained notice bytes."
