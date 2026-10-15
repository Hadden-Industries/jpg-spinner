<# Exercise the real SDK on a retained three-architecture development bundle.
   An unsigned fixture proves structure/payload inspection, never release trust. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$InspectionDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not [IO.File]::Exists($BundlePath)) { throw 'The real bundle fixture is absent.' }
$before = (Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash
& (Join-Path $repositoryRoot 'scripts/Expand-JpgSpinnerBundle.ps1') `
    -BundlePath $BundlePath -InspectionDirectory $InspectionDirectory
foreach ($architecture in @('x86','x64','arm64')) {
    $payload = Join-Path $InspectionDirectory "payloads/$architecture"
    foreach ($required in @('AppxManifest.xml','AppxBlockMap.xml','JpgSpinner.exe','resources.pri')) {
        if (-not [IO.File]::Exists((Join-Path $payload $required))) {
            throw "Bundle inspection did not extract $architecture payload: $required"
        }
    }
    $inventoryPath = Join-Path $InspectionDirectory "inventories/$architecture.json"
    $inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
    $executable = @($inventory.files | Where-Object relativePath -CEQ 'JpgSpinner.exe')
    if ($executable.Count -ne 1 -or $executable[0].sha256 -cne
        (Get-FileHash -LiteralPath (Join-Path $payload 'JpgSpinner.exe') -Algorithm SHA256).Hash.ToLowerInvariant()) {
        throw 'The extracted executable is absent or incorrectly hashed in the payload inventory.'
    }
}
if ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash -cne $before) {
    throw 'Bundle inspection changed the original container.'
}
Write-Output 'PASS: native bundle inspection extracts and hashes all three real architecture payloads without changing the input.'
