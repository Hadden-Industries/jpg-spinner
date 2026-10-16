<# The positive fixture consists of the separately qualified real Release
   x86/x64/ARM64 application packages; never mislabeled files or mocked ZIPs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$PackagePaths,
    [Parameter(Mandatory)][string]$OutputPath,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$InspectionDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ($PackagePaths.Count -ne 3) { throw 'The positive bundle fixture requires three qualified architecture packages.' }
foreach ($package in $PackagePaths) {
    if (-not (Test-Path -LiteralPath $package -PathType Leaf)) { throw 'A qualified positive package fixture is absent.' }
}
if ((Test-Path -LiteralPath $OutputPath) -or (Test-Path -LiteralPath $WorkingDirectory) -or
    (Test-Path -LiteralPath $InspectionDirectory)) { throw 'Bundle contract outputs must be fresh.' }
& (Join-Path $root 'scripts/New-MsixBundle.ps1') -PackagePaths $PackagePaths `
    -OutputPath $OutputPath -WorkingDirectory $WorkingDirectory
if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf) -or (Get-Item -LiteralPath $OutputPath).Length -eq 0) {
    throw 'Native composition did not emit an actual MSIX bundle.'
}
$toolchain = & (Join-Path $root 'scripts/Resolve-MsixToolchain.ps1')
$makeAppx = $toolchain.MakeAppxExecutablePath
& $makeAppx unbundle /p $OutputPath /d $InspectionDirectory /no
if ($LASTEXITCODE -ne 0) { throw 'Native MakeAppx refused the emitted bundle.' }
$manifest = [Xml.XmlDocument]::new()
$manifest.Load((Join-Path $InspectionDirectory 'AppxMetadata/AppxBundleManifest.xml'))
$namespaces = [Xml.XmlNamespaceManager]::new($manifest.NameTable)
$namespaces.AddNamespace('b','http://schemas.microsoft.com/appx/2013/bundle')
$identity = $manifest.SelectSingleNode('/b:Bundle/b:Identity',$namespaces)
if ($null -eq $identity -or $identity.GetAttribute('Name') -cne 'HaddenIndustriesLtd.JPGSpinner' -or
    $identity.GetAttribute('Publisher') -cne 'CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3' -or
    $identity.GetAttribute('Version') -cne '2026.2.0.0') { throw 'Actual native bundle changed the approved Store identity/version.' }
if ([Version]$identity.GetAttribute('Version') -le [Version]'2015.616.2316.3521') {
    throw 'Actual native bundle must exceed the observed published bundle baseline.'
}
$packages = $manifest.SelectNodes('/b:Bundle/b:Packages/b:Package[@Type="application"]',$namespaces)
if ($packages.Count -ne 3) { throw 'Actual bundle must contain exactly three application architecture packages.' }
$architectures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($package in $packages) {
    [void]$architectures.Add($package.GetAttribute('Architecture'))
    if ($package.GetAttribute('Version') -cne '2.0.0.0') { throw 'Bundling must preserve the independent application version.' }
}
if (-not $architectures.SetEquals([string[]]@('x86','x64','arm64'))) { throw 'Actual bundle architecture set is not x86/x64/ARM64.' }
Write-Output 'PASS: native bundle reader confirms fixed 2026.2.0.0 bundle, preserved 2.0.0.0 applications and all three architectures.'
