<# Missing/duplicate package selections must refuse before output or staging. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExistingPackagePath, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer = Join-Path $root 'scripts/New-MsixBundle.ps1'
$package = [IO.Path]::GetFullPath($ExistingPackagePath)
if (-not (Test-Path -LiteralPath $package -PathType Leaf)) { throw 'The positive native package fixture is absent.' }
$before = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
$output = Join-Path $OwnedOutputRoot 'refused-input.msixbundle'
$working = Join-Path $OwnedOutputRoot 'refused-input-staging'
foreach ($case in @(
    @{Paths=@($package); Diagnostic='exactly three distinct architecture packages'},
    @{Paths=@($package,$package,$package); Diagnostic='distinct package paths'}
)) {
    $errorMessage = $null
    try { & $producer -PackagePaths $case.Paths -OutputPath $output -WorkingDirectory $working }
    catch { $errorMessage = $_.Exception.Message }
    if ($null -eq $errorMessage -or -not $errorMessage.Contains($case.Diagnostic, [StringComparison]::Ordinal)) {
        throw "Native bundle input refusal is absent or imprecise: $errorMessage"
    }
    if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $working)) {
        throw 'Bundle input refusal created output or staging.'
    }
}
if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -cne $before) {
    throw 'Bundle refusal changed its qualified source package.'
}
Write-Output 'PASS: missing/duplicate architecture-package selections refuse before effects; source package unchanged.'
