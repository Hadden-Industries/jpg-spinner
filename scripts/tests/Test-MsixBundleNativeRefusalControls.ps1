<# Three distinct paths are not three architectures. Native MakeAppx must
   reject a repeated-architecture set without changing the qualified input. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExistingPackagePath, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixtureRoot = Join-Path $OwnedOutputRoot 'same-architecture-control'
if (Test-Path -LiteralPath $fixtureRoot) { throw 'Native refusal fixture must be fresh.' }
if (-not (Test-Path -LiteralPath $ExistingPackagePath -PathType Leaf)) { throw 'Qualified package fixture is absent.' }
$sourceHash = (Get-FileHash -LiteralPath $ExistingPackagePath -Algorithm SHA256).Hash
[void][IO.Directory]::CreateDirectory($fixtureRoot)
$packagePaths = [Collections.Generic.List[string]]::new()
foreach ($index in 1..3) {
    $copy = Join-Path $fixtureRoot "same-architecture-$index.msix"
    Copy-Item -LiteralPath $ExistingPackagePath -Destination $copy
    if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $sourceHash) { throw 'Native negative fixture copy differs from its input.' }
    $packagePaths.Add($copy)
}
$message = $null
try {
    & (Join-Path $repositoryRoot 'scripts/New-MsixBundle.ps1') -PackagePaths $packagePaths.ToArray() `
        -OutputPath (Join-Path $fixtureRoot 'refused.msixbundle') -WorkingDirectory (Join-Path $fixtureRoot 'staging')
}
catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Native MakeAppx refused the architecture package set.',[StringComparison]::Ordinal)) {
    throw "Native semantic validation did not reject the repeated architecture: $message"
}
if ((Get-FileHash -LiteralPath $ExistingPackagePath -Algorithm SHA256).Hash -cne $sourceHash) { throw 'Native refusal changed the source package.' }
# A failed native producer may retain incomplete output/staging for diagnosis;
# refusal is not an assertion of atomic filesystem cleanup or publication.
Write-Output 'PASS: native MakeAppx rejects distinct paths containing one repeated architecture; qualified input unchanged.'
