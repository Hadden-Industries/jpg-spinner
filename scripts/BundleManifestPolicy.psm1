<# Product policy on SDK-extracted manifests, not a ZIP or MSIX schema parser.
   Microsoft MakeAppx remains the consuming native container reader. #>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-JpgSpinnerBundlePackageDeclaration {
    <# Return the closed three-architecture application set. Only single file
       names are allowed because extraction never resolves manifest paths into
       arbitrary filesystem locations. Platform XML parsing owns XML syntax. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][Xml.XmlDocument]$BundleManifest)
    $namespaces = [Xml.XmlNamespaceManager]::new($BundleManifest.NameTable)
    $namespaces.AddNamespace('b','http://schemas.microsoft.com/appx/2013/bundle')
    if ($BundleManifest.SelectNodes('/b:Bundle/b:Identity',$namespaces).Count -ne 1) {
        throw 'Bundle policy requires exactly one identity.'
    }
    $packages = $BundleManifest.SelectNodes('/b:Bundle/b:Packages/b:Package',$namespaces)
    if ($packages.Count -ne 3) { throw 'Bundle policy requires exactly three application packages.' }
    $architectures = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $fileNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($package in $packages) {
        $architecture = $package.GetAttribute('Architecture')
        $fileName = $package.GetAttribute('FileName')
        if ($package.GetAttribute('Type') -cne 'application' -or
            $architecture -cnotin @('x86','x64','arm64') -or -not $architectures.Add($architecture)) {
            throw 'Bundle policy requires distinct x86/x64/arm64 application packages.'
        }
        # Native Windows filename rules reject separators, alternate streams and
        # special characters. Require the extension and uniqueness independently
        # of architecture, so two declarations cannot consume one output path.
        if ([string]::IsNullOrWhiteSpace($fileName) -or $fileName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
            [IO.Path]::GetExtension($fileName) -cne '.msix' -or -not $fileNames.Add($fileName)) {
            throw 'Bundle policy requires unique simple .msix file names.'
        }
        [pscustomobject]@{Architecture=$architecture; FileName=$fileName; Version=$package.GetAttribute('Version')}
    }
}

function Assert-JpgSpinnerBundlePackageIdentity {
    <# Enforce bundle/package agreement after the package has passed the existing
       Store/AppContainer policy. Bundle signing covers inner packages; separate
       inner signatures are deliberately not required by this structural check.
       https://learn.microsoft.com/windows/msix/package/signing-package-overview #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][Xml.XmlDocument]$BundleManifest,
        [Parameter(Mandatory)][Xml.XmlDocument]$PackageManifest,
        [Parameter(Mandatory)][string]$Architecture,
        [Parameter(Mandatory)][string]$DeclaredVersion,
        [Parameter(Mandatory)][string]$ExpectedBundleVersion
    )
    $bundleNamespaces = [Xml.XmlNamespaceManager]::new($BundleManifest.NameTable)
    $bundleNamespaces.AddNamespace('b','http://schemas.microsoft.com/appx/2013/bundle')
    $packageNamespaces = [Xml.XmlNamespaceManager]::new($PackageManifest.NameTable)
    $packageNamespaces.AddNamespace('p','http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $bundleIdentities = $BundleManifest.SelectNodes('/b:Bundle/b:Identity',$bundleNamespaces)
    $packageIdentities = $PackageManifest.SelectNodes('/p:Package/p:Identity',$packageNamespaces)
    if ($bundleIdentities.Count -ne 1 -or $packageIdentities.Count -ne 1) { throw 'Bundle/package identity is absent or ambiguous.' }
    # Bundle Identity.Version is independent of each contained Package.Version.
    # Enforce the authored outer version separately, rather than rejecting a
    # native bundle merely because its application uses another version epoch.
    # https://learn.microsoft.com/uwp/schemas/bundlemanifestschema/element-identity
    if ($bundleIdentities[0].GetAttribute('Version') -cne $ExpectedBundleVersion) {
        throw 'Bundle identity differs from the expected bundle version.'
    }
    foreach ($attribute in @('Name','Publisher')) {
        if ($bundleIdentities[0].GetAttribute($attribute) -cne $packageIdentities[0].GetAttribute($attribute)) {
            throw "Bundle/package identity differs: $attribute"
        }
    }
    if ($packageIdentities[0].GetAttribute('ProcessorArchitecture') -cne $Architecture -or
        $packageIdentities[0].GetAttribute('Version') -cne $DeclaredVersion) {
        throw 'Package identity differs from its bundle declaration.'
    }
}

Export-ModuleMember -Function Get-JpgSpinnerBundlePackageDeclaration,Assert-JpgSpinnerBundlePackageIdentity
