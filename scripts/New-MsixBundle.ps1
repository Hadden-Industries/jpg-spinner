<# Compose qualified architecture packages with the owning native MSIX bundler.
   Does not compile, sign, install or submit. Native MakeAppx owns MSIX semantics. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$PackagePaths,
    [Parameter(Mandatory)][string]$OutputPath,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($PackagePaths.Count -ne 3) {
    throw 'Bundle requires exactly three distinct architecture packages.'
}
$canonicalPackages = [Collections.Generic.List[string]]::new()
$uniquePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($path in $PackagePaths) {
    $canonical = [IO.Path]::GetFullPath($path)
    if (-not $uniquePaths.Add($canonical)) { throw 'Bundle requires distinct package paths.' }
    if (-not (Test-Path -LiteralPath $canonical -PathType Leaf) -or
        -not [IO.Path]::GetExtension($canonical).Equals('.msix',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Each bundle input must be an existing native .msix package.'
    }
    $canonicalPackages.Add($canonical)
}
$canonicalOutput = [IO.Path]::GetFullPath($OutputPath)
$canonicalWorkingDirectory = [IO.Path]::GetFullPath($WorkingDirectory)
if (-not [IO.Path]::GetExtension($canonicalOutput).Equals('.msixbundle',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Bundle output must use the .msixbundle extension.'
}
if ((Test-Path -LiteralPath $canonicalOutput) -or (Test-Path -LiteralPath $canonicalWorkingDirectory)) {
    throw 'Bundle output and staging directory must be fresh and absent.'
}
$packageNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($package in $canonicalPackages) {
    if (-not $packageNames.Add([IO.Path]::GetFileName($package))) { throw 'Bundle staging requires distinct package file names.' }
}
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MsixToolchain.ps1')
Push-Location -LiteralPath $repositoryRoot
try {
    $makeAppxPath = $toolchain.MakeAppxExecutablePath
    [void][IO.Directory]::CreateDirectory($canonicalWorkingDirectory)
    foreach ($package in $canonicalPackages) {
        Copy-Item -LiteralPath $package -Destination (Join-Path $canonicalWorkingDirectory ([IO.Path]::GetFileName($package))) -ErrorAction Stop
    }
    # Keep native semantic validation enabled. /no forbids overwrites; /bv
    # prevents the tool's implicit wall-clock bundle version. MakeAppx owns
    # package identity/architecture compatibility, not a parallel ZIP parser.
    # https://learn.microsoft.com/windows/msix/package/create-app-package-with-makeappx-tool
    & $makeAppxPath bundle /d $canonicalWorkingDirectory /p $canonicalOutput /bv $toolchain.AppxBundleManifestVersion /no
    if ($LASTEXITCODE -ne 0) { throw 'Native MakeAppx refused the architecture package set.' }
    if (-not [IO.File]::Exists($canonicalOutput) -or (Get-Item -LiteralPath $canonicalOutput).Length -eq 0) {
        throw 'Native MakeAppx did not emit a nonempty MSIX bundle.'
    }
}
finally { Pop-Location }
Write-Output $canonicalOutput
