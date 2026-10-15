<# Cheap refusal controls must run before any clone, restore or build. Positive
   independent-build qualification uses the real locked compiler and MSIX tools. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer=Join-Path $root 'scripts/Invoke-IndependentPackageBuildComparison.ps1'
$fixture=Join-Path $OwnedOutputRoot ('independent-build-comparison-refusals-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$working=Join-Path $fixture work
$output=Join-Path $fixture output
$message=$null
try {& $producer -SourceCommit ('0'*40) -WorkingDirectory $working -OutputDirectory $output | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('selected source commit',[StringComparison]::Ordinal)) {throw "Wrong commit did not fail at the source boundary: $message"}
if ((Test-Path -LiteralPath $working) -or (Test-Path -LiteralPath $output)) {throw 'A refused source created build outputs.'}
Write-Output 'PASS: wrong candidate refuses before cloning or native build execution.'
$commit=git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) {throw 'Cannot observe selected fixture commit.'}
[void][IO.Directory]::CreateDirectory($output)
$message=$null
try {& $producer -SourceCommit $commit -WorkingDirectory $working -OutputDirectory $output | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('fresh and absent',[StringComparison]::Ordinal)) {throw "Existing output did not fail before native execution: $message"}
if (Test-Path -LiteralPath $working) {throw 'Existing-output refusal created a clone.'}
$message=$null
try {& $producer -SourceCommit $commit -WorkingDirectory $working -OutputDirectory (Join-Path $working result) | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('destination parents must exist',[StringComparison]::Ordinal)) {throw "Nested output did not fail before native execution: $message"}
Write-Output 'PASS: existing/nested outputs cannot start independent builds.'
