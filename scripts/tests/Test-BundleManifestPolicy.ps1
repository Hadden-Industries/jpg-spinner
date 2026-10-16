<# Independent product-policy examples; XML DOM owns syntax. These are not
   native container/schema tests, which use the retained actual MSIX bundle. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'BundleManifestPolicy.psm1') -Force
[xml]$bundle = @'
<Bundle xmlns="http://schemas.microsoft.com/appx/2013/bundle">
 <Identity Name="Example.Product" Publisher="CN=Example" Version="2.0.0.0"/>
 <Packages>
  <Package Type="application" Architecture="x86" FileName="x86.msix" Version="2.0.0.0"/>
  <Package Type="application" Architecture="x64" FileName="x64.msix" Version="2.0.0.0"/>
  <Package Type="application" Architecture="arm64" FileName="arm64.msix" Version="2.0.0.0"/>
 </Packages>
</Bundle>
'@
[xml]$package = @'
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10">
 <Identity Name="Example.Product" Publisher="CN=Example" Version="2.0.0.0" ProcessorArchitecture="x64"/>
</Package>
'@
$declarations = @(Get-JpgSpinnerBundlePackageDeclaration -BundleManifest $bundle)
if ($declarations.Count -ne 3 -or $declarations[1].FileName -cne 'x64.msix') { throw 'Accepted declaration set changed.' }
Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $bundle -PackageManifest $package -Architecture x64 -DeclaredVersion 2.0.0.0 -ExpectedBundleVersion 2.0.0.0
# The native bundle Identity and each inner Package declaration have distinct
# Version attributes. A fixed outer epoch must not force application versions
# to impersonate the legacy bundle's date-based numbering scheme.
$distinctBundleVersion = [xml]$bundle.CloneNode($true)
$distinctBundleVersion.Bundle.Identity.SetAttribute('Version','2026.2.0.0')
Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $distinctBundleVersion -PackageManifest $package -Architecture x64 -DeclaredVersion 2.0.0.0 -ExpectedBundleVersion 2026.2.0.0
function Assert-Refusal([scriptblock]$Action, [string]$Diagnostic) {
    $message = $null
    try { & $Action | Out-Null } catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($Diagnostic,[StringComparison]::Ordinal)) {
        throw "Expected '$Diagnostic', received '$message'."
    }
}
foreach ($case in @(
    @{Attribute='Architecture';Value='neutral';Diagnostic='distinct x86/x64/arm64'},
    @{Attribute='Architecture';Value='x64';Diagnostic='distinct x86/x64/arm64'},
    @{Attribute='Type';Value='resource';Diagnostic='distinct x86/x64/arm64'},
    @{Attribute='FileName';Value='../escape.msix';Diagnostic='unique simple .msix'},
    @{Attribute='FileName';Value='C:\escape.msix';Diagnostic='unique simple .msix'},
    @{Attribute='FileName';Value='x64.msix:stream';Diagnostic='unique simple .msix'},
    @{Attribute='FileName';Value='X64.msix';Diagnostic='unique simple .msix'}
)) {
    $candidate = [xml]$bundle.CloneNode($true)
    $candidate.Bundle.Packages.Package[0].SetAttribute($case.Attribute,$case.Value)
    Assert-Refusal { Get-JpgSpinnerBundlePackageDeclaration -BundleManifest $candidate } $case.Diagnostic
}
$missing = [xml]$bundle.CloneNode($true)
[void]$missing.Bundle.Packages.RemoveChild($missing.Bundle.Packages.LastChild)
Assert-Refusal { Get-JpgSpinnerBundlePackageDeclaration -BundleManifest $missing } 'exactly three'
$duplicateIdentity = [xml]$bundle.CloneNode($true)
[void]$duplicateIdentity.Bundle.AppendChild($duplicateIdentity.Bundle.Identity.CloneNode($true))
Assert-Refusal { Get-JpgSpinnerBundlePackageDeclaration -BundleManifest $duplicateIdentity } 'exactly one identity'
foreach ($attribute in @('Name','Publisher','Version','ProcessorArchitecture')) {
    $candidate = [xml]$package.CloneNode($true)
    $candidate.Package.Identity.SetAttribute($attribute,'Different')
    Assert-Refusal {
        Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $bundle -PackageManifest $candidate -Architecture x64 -DeclaredVersion 2.0.0.0 -ExpectedBundleVersion 2.0.0.0
    } 'differs'
}
Assert-Refusal {
    Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $bundle -PackageManifest $package -Architecture x64 -DeclaredVersion 2.0.0.1 -ExpectedBundleVersion 2.0.0.0
} 'differs'
# Independence is not permission to accept an unapproved outer version. A
# downgrade and an unexpected higher version both fail the exact contract.
foreach ($unexpectedVersion in @('2015.616.2316.3521','2.0.0.0','2026.2.0.1')) {
    $candidate = [xml]$distinctBundleVersion.CloneNode($true)
    $candidate.Bundle.Identity.SetAttribute('Version',$unexpectedVersion)
    Assert-Refusal {
        Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $candidate -PackageManifest $package -Architecture x64 -DeclaredVersion 2.0.0.0 -ExpectedBundleVersion 2026.2.0.0
    } 'expected bundle version'
}
Write-Output 'PASS: independent bundle/application versions; exact outer version, declarations and package identities remain enforced.'
