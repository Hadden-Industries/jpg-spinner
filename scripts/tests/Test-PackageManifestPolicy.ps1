<# Native XML policy tests; fixtures are in-memory and never deploy a package. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'PackageManifestPolicy.psm1') -Force
# Independent accepted requirements, not a golden copy generated from production.
[xml]$manifest = @'
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
 xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10">
 <Identity Name="HaddenIndustriesLtd.JPGSpinner" Publisher="CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3" Version="2.0.0.0"/>
 <Dependencies><TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19045.0" MaxVersionTested="10.0.28000.0"/></Dependencies>
 <Applications><Application Id="App" uap10:TrustLevel="appContainer" uap10:RuntimeBehavior="packagedClassicApp"/></Applications>
 <Capabilities/>
</Package>
'@
[xml]$association = @'
<StoreAssociation xmlns="http://schemas.microsoft.com/appx/2010/storeassociation">
 <Publisher>CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3</Publisher>
 <ProductReservedInfo><MainPackageIdentityName>HaddenIndustriesLtd.JPGSpinner</MainPackageIdentityName></ProductReservedInfo>
 <PackageInfoList LandingUrl="https://dev.windows.com/dashboard/Application?appId=9NBLGGH3TVGW"/>
</StoreAssociation>
'@
Assert-JpgSpinnerPackageManifest -Manifest $manifest -StoreAssociation $association
function Assert-PolicyRefusal {
    param([xml]$Candidate, [xml]$Association = $association, [string]$Diagnostic)
    $message = $null
    try { Assert-JpgSpinnerPackageManifest -Manifest $Candidate -StoreAssociation $Association }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($Diagnostic, [StringComparison]::Ordinal)) {
        throw "Expected '$Diagnostic', got '$message'."
    }
}
$wrongIdentity = [xml]$manifest.CloneNode($true)
$wrongIdentity.Package.Identity.SetAttribute('Name', 'Different.Product')
Assert-PolicyRefusal -Candidate $wrongIdentity -Diagnostic 'Identity.Name'
$wrongTrust = [xml]$manifest.CloneNode($true)
$wrongTrust.Package.Applications.Application.SetAttribute('TrustLevel',
    'http://schemas.microsoft.com/appx/manifest/uap/windows10/10', 'mediumIL') | Out-Null
Assert-PolicyRefusal -Candidate $wrongTrust -Diagnostic 'TrustLevel'
$conflictingEntryPoint = [xml]$manifest.CloneNode($true)
$conflictingEntryPoint.Package.Applications.Application.SetAttribute('EntryPoint', 'Windows.FullTrustApplication')
Assert-PolicyRefusal -Candidate $conflictingEntryPoint -Diagnostic 'redundant EntryPoint'
foreach ($capabilityName in @('runFullTrust', 'internetClient', 'broadFileSystemAccess', 'packageManagement')) {
    $capability = [xml]$manifest.CloneNode($true)
    $entry = $capability.CreateElement('Capability', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities')
    $entry.SetAttribute('Name', $capabilityName)
    [void]$capability.DocumentElement.LastChild.AppendChild($entry)
    Assert-PolicyRefusal -Candidate $capability -Diagnostic 'Capabilities'
}
$duplicate = [xml]$manifest.CloneNode($true)
[void]$duplicate.DocumentElement.AppendChild($duplicate.Package.Identity.CloneNode($true))
Assert-PolicyRefusal -Candidate $duplicate -Diagnostic 'Exactly one package Identity'
$wrongAssociation = [xml]$association.CloneNode($true)
$wrongAssociation.StoreAssociation.PackageInfoList.SetAttribute('LandingUrl',
    'https://dev.windows.com/dashboard/Application?other=9NBLGGH3TVGW&appId=OTHER')
Assert-PolicyRefusal -Candidate $manifest -Association $wrongAssociation -Diagnostic 'appId'
$extraFamily = [xml]$manifest.CloneNode($true)
$family = $extraFamily.Package.Dependencies.TargetDeviceFamily.CloneNode($true)
$family.SetAttribute('Name', 'Windows.Universal')
[void]$extraFamily.Package.Dependencies.AppendChild($family)
Assert-PolicyRefusal -Candidate $extraFamily -Diagnostic 'Exactly one Windows.Desktop'
$fileAssociation = [xml]$manifest.CloneNode($true)
$extensions = $fileAssociation.CreateElement('Extensions', $fileAssociation.DocumentElement.NamespaceURI)
$extension = $fileAssociation.CreateElement('Extension', $fileAssociation.DocumentElement.NamespaceURI)
$extension.SetAttribute('Category', 'windows.fileTypeAssociation')
[void]$extensions.AppendChild($extension)
[void]$fileAssociation.DocumentElement.AppendChild($extensions)
Assert-PolicyRefusal -Candidate $fileAssociation -Diagnostic 'in-process WinRT'
Write-Output 'PASS: package policy accepts the independent contract and rejects changed identity/trust/capabilities/duplicate identity/wrong Store query.'
