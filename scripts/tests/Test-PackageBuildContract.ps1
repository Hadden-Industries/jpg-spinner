<# Real unsigned MSIX production contract. Requires already restored native
   tools and an explicitly selected owned output directory. No signing/install. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('x86','x64','ARM64')][string]$Architecture = 'x64'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Package contract test requires a fresh owned output directory.' }
$powershell = (Get-Process -Id $PID).Path
$buildOutput = & $powershell -NoProfile -File (Join-Path $root 'scripts/Invoke-Build.ps1') `
    -Configuration $Configuration -Architecture $Architecture -BuildEntryPoint PackagedApplication `
    -UnsignedPackageOutputDirectory $output 2>&1
if ($LASTEXITCODE -ne 0) { throw "Native package producer failed: $($buildOutput -join "`n")" }
[Console]::Out.WriteLine($buildOutput -join "`n")
if (-not (Test-Path -LiteralPath $output -PathType Container)) {
    throw 'Native application build did not produce the requested unsigned package directory.'
}
$packages = @(Get-ChildItem -LiteralPath $output -Filter 'JpgSpinner.App_*.msix' -File -Recurse)
if ($packages.Count -ne 1) { throw 'Native application build must emit exactly one application .msix, not just framework sidecars.' }
foreach ($package in $packages) {
    if ($package.Length -eq 0) { throw 'Native package output is empty.' }
}
Write-Output "PASS: native $Architecture $Configuration build emitted unsigned MSIX output."
