<# Extract a caller-qualified, quiescent bundle with the owning Windows SDK.
   This structural inspection is NOT signature/trust or deployment validation.
   Release callers must verify the outer signed bundle before invoking it;
   unsigned development bundles remain useful for payload reproducibility. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$InspectionDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$bundle = [IO.Path]::GetFullPath($BundlePath)
$inspection = [IO.Path]::GetFullPath($InspectionDirectory)
if (-not [IO.File]::Exists($bundle) -or [IO.Path]::GetExtension($bundle) -cne '.msixbundle') {
    throw 'The exact .msixbundle input is absent.'
}
if (Test-Path -LiteralPath $inspection) { throw 'Bundle inspection requires a fresh absent directory.' }

function Assert-OrdinaryPathAncestors([string]$Path) {
    # Check before opening files or creating outputs. A fresh leaf alone does not
    # rule out an ancestor junction redirecting the caller's inspection tree.
    $current = $Path
    while (-not [string]::IsNullOrEmpty($current)) {
        if ((Test-Path -LiteralPath $current) -and
            ([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Bundle inspection refuses reparse-point inputs or output ancestors.'
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}
Assert-OrdinaryPathAncestors $bundle
Assert-OrdinaryPathAncestors $inspection
$parent = [IO.Path]::GetDirectoryName($inspection)
if (-not [IO.Directory]::Exists($parent)) { throw 'Bundle inspection requires an existing owned parent directory.' }
Import-Module (Join-Path $PSScriptRoot 'BundleManifestPolicy.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PackageManifestPolicy.psm1') -Force
$association = [Xml.XmlDocument]::new()
$association.Load((Join-Path $repositoryRoot 'src/JpgSpinner.App/Package.StoreAssociation.xml'))
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MsixToolchain.ps1')
# Share read only for the entire operation: native readers can open the exact
# container, while concurrent Windows writers/deleters cannot replace it.
$bundleHandle = [IO.File]::OpenRead($bundle)
Push-Location -LiteralPath $repositoryRoot
try {
    $makeAppx = $toolchain.MakeAppxExecutablePath
    $diagnostics = Join-Path $inspection 'diagnostics'
    [void][IO.Directory]::CreateDirectory($diagnostics)
    # Preserve native output without mixing diagnostic prose into the script's
    # success value. Never use /nv (skip validation) or /o (overwrite).
    # https://learn.microsoft.com/windows/msix/package/create-app-package-with-makeappx-tool
    & $makeAppx unbundle /p $bundle /d (Join-Path $inspection 'bundle') /no > (Join-Path $diagnostics 'unbundle.log') 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Native MakeAppx refused the bundle.' }
    $bundleDirectory = Join-Path $inspection 'bundle'
    $inventories = Join-Path $inspection 'inventories'
    [void][IO.Directory]::CreateDirectory($inventories)
    # Reuse the established non-following traversal and native SHA-256 writer.
    # Inventory lives outside every payload; no generated SBOM inventories itself.
    & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $bundleDirectory `
        -OutputPath (Join-Path $inventories 'bundle.json') | Out-Null
    $bundleManifest = [Xml.XmlDocument]::new()
    $bundleManifest.Load((Join-Path $bundleDirectory 'AppxMetadata/AppxBundleManifest.xml'))
    $declarations = @(Get-JpgSpinnerBundlePackageDeclaration -BundleManifest $bundleManifest)
    $actualPackages = @(Get-ChildItem -LiteralPath $bundleDirectory -File -Filter '*.msix')
    if ($actualPackages.Count -ne $declarations.Count) { throw 'Bundle contains an undeclared or missing application package.' }
    foreach ($declaration in $declarations) {
        $package = Join-Path $bundleDirectory $declaration.FileName
        if (-not [IO.File]::Exists($package)) { throw 'Bundle declaration refers to an absent package.' }
        $payload = Join-Path $inspection "payloads/$($declaration.Architecture)"
        & $makeAppx unpack /p $package /d $payload /no > (Join-Path $diagnostics "unpack-$($declaration.Architecture).log") 2>&1
        if ($LASTEXITCODE -ne 0) { throw 'Native MakeAppx refused an architecture package.' }
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $payload `
            -OutputPath (Join-Path $inventories "$($declaration.Architecture).json") | Out-Null
        $packageManifest = [Xml.XmlDocument]::new()
        $packageManifest.Load((Join-Path $payload 'AppxManifest.xml'))
        Assert-JpgSpinnerPackageManifest -Manifest $packageManifest -StoreAssociation $association
        Assert-JpgSpinnerBundlePackageIdentity -BundleManifest $bundleManifest -PackageManifest $packageManifest `
            -Architecture $declaration.Architecture -DeclaredVersion $declaration.Version `
            -ExpectedBundleVersion $toolchain.AppxBundleManifestVersion
    }
}
finally { Pop-Location; $bundleHandle.Dispose() }
# No success value is emitted on refusal. Partial owned outputs remain diagnostic
# evidence, not an accepted input for subsequent SBOM/release promotion.
Write-Output $inspection
