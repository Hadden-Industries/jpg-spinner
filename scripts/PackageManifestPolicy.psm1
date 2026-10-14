Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-JpgSpinnerPackageManifest {
    <#
    .SYNOPSIS
    Enforces JPG Spinner's accepted Store identity and least-privilege contract.
    .DESCRIPTION
    Takes XML documents already parsed by .NET. These are product policy checks,
    not an implementation of Microsoft's manifest schema. The SDK manifest reader
    and MakeAppx remain responsible for platform validity and package validation.
    No deployment, signing, trust changes or filesystem mutation is performed.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][System.Xml.XmlDocument]$Manifest,
        [Parameter(Mandatory)][System.Xml.XmlDocument]$StoreAssociation
    )
    $namespaces = [System.Xml.XmlNamespaceManager]::new($Manifest.NameTable)
    $namespaces.AddNamespace('p', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $namespaces.AddNamespace('uap10', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/10')
    $failures = [System.Collections.Generic.List[string]]::new()
    $identities = $Manifest.SelectNodes('/p:Package/p:Identity', $namespaces)
    if ($identities.Count -ne 1) { $failures.Add('Exactly one package Identity is required.') }
    else {
        foreach ($expected in ([ordered]@{
            Name = 'HaddenIndustriesLtd.JPGSpinner'
            Publisher = 'CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3'
            Version = '2.0.0.0'
        }).GetEnumerator()) {
            if ($identities.Item(0).GetAttribute($expected.Key) -cne $expected.Value) {
                $failures.Add("Package Identity.$($expected.Key) must equal '$($expected.Value)'.")
            }
        }
    }
    $families = $Manifest.SelectNodes('/p:Package/p:Dependencies/p:TargetDeviceFamily', $namespaces)
    if ($families.Count -ne 1) { $failures.Add('Exactly one Windows.Desktop TargetDeviceFamily is required.') }
    else {
        foreach ($expected in ([ordered]@{
            Name = 'Windows.Desktop'; MinVersion = '10.0.19045.0'; MaxVersionTested = '10.0.28000.0'
        }).GetEnumerator()) {
            if ($families.Item(0).GetAttribute($expected.Key) -cne $expected.Value) {
                $failures.Add("TargetDeviceFamily.$($expected.Key) must equal '$($expected.Value)'.")
            }
        }
    }
    $applications = $Manifest.SelectNodes('/p:Package/p:Applications/p:Application', $namespaces)
    if ($applications.Count -ne 1) { $failures.Add('Exactly one application is required.') }
    else {
        $application = $applications.Item(0)
        if ($application.GetAttribute('Id') -cne 'App') { $failures.Add('Application.Id must equal App.') }
        # Minimum OS 19045 supports these modern activation attributes. A legacy
        # EntryPoint is redundant, and the C++ full-trust token contradicts them.
        # https://learn.microsoft.com/uwp/schemas/appxpackage/uapmanifestschema/element-application
        if ($application.HasAttribute('EntryPoint')) {
            $failures.Add('Use uap10 activation attributes without a redundant EntryPoint.')
        }
        foreach ($expected in ([ordered]@{TrustLevel = 'appContainer'; RuntimeBehavior = 'packagedClassicApp'}).GetEnumerator()) {
            if ($application.GetAttribute($expected.Key, $namespaces.LookupNamespace('uap10')) -cne $expected.Value) {
                $failures.Add("Application.uap10:$($expected.Key) must equal '$($expected.Value)'.")
            }
        }
    }
    # No operation currently requires a declared capability or extension. This
    # closed contract also rejects nested/alternate-namespace capability entries,
    # broad filesystem/network access and additional application entry points.
    $capabilities = $Manifest.SelectNodes('/p:Package/p:Capabilities', $namespaces)
    if ($capabilities.Count -ne 1 -or $capabilities.Item(0).SelectNodes('*').Count -ne 0) {
        $failures.Add('Capabilities must be present and empty; all access is picker-mediated.')
    }
    if ($Manifest.SelectNodes('//p:Extensions/*[not(@Category="windows.activatableClass.inProcessServer")]', $namespaces).Count -ne 0) {
        # Native WinRT registration can be emitted by the SDK for its own
        # components. It is not a second app entry or a file-type association.
        $failures.Add('Only native in-process WinRT activation extensions are permitted.')
    }
    $associationNamespaces = [System.Xml.XmlNamespaceManager]::new($StoreAssociation.NameTable)
    $associationNamespaces.AddNamespace('s', 'http://schemas.microsoft.com/appx/2010/storeassociation')
    foreach ($expected in ([ordered]@{
        '/s:StoreAssociation/s:Publisher' = 'CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3'
        '/s:StoreAssociation/s:ProductReservedInfo/s:MainPackageIdentityName' = 'HaddenIndustriesLtd.JPGSpinner'
    }).GetEnumerator()) {
        $nodes = $StoreAssociation.SelectNodes($expected.Key, $associationNamespaces)
        if ($nodes.Count -ne 1 -or $nodes.Item(0).InnerText -cne $expected.Value) {
            $failures.Add("Store association must retain '$($expected.Value)'.")
        }
    }
    $packageLists = $StoreAssociation.SelectNodes('/s:StoreAssociation/s:PackageInfoList', $associationNamespaces)
    if ($packageLists.Count -ne 1) { $failures.Add('Store association requires one PackageInfoList.') }
    else {
        # URI parsing owns escaping/query semantics; do not identify a product by
        # a substring that could occur in another query key or URL fragment.
        $uri = $null
        if (-not [Uri]::TryCreate($packageLists.Item(0).GetAttribute('LandingUrl'), [UriKind]::Absolute, [ref]$uri) -or
            [System.Web.HttpUtility]::ParseQueryString($uri.Query).Get('appId') -cne '9NBLGGH3TVGW') {
            $failures.Add('Store association appId must equal 9NBLGGH3TVGW.')
        }
    }
    if ($failures.Count -ne 0) { throw ($failures -join [Environment]::NewLine) }
}

Export-ModuleMember -Function Assert-JpgSpinnerPackageManifest
