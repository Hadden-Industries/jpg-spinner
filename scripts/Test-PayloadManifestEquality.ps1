<# Compare canonical manifests emitted by New-PayloadManifest.ps1.
   A match proves payload bytes/path/length equality, not container identity,
   signature validity or the correctness of either independent build. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FirstManifestPath,
    [Parameter(Mandatory)][string]$SecondManifestPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
foreach ($path in @($FirstManifestPath,$SecondManifestPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'Both canonical payload manifests must exist.' }
}
if ((Get-FileHash -LiteralPath $FirstManifestPath -Algorithm SHA256).Hash -cne
    (Get-FileHash -LiteralPath $SecondManifestPath -Algorithm SHA256).Hash) {
    throw 'Canonical payload manifests differ.'
}
Write-Output 'PASS: canonical payload manifests are byte-identical.'
