<# Real independent copied roots must compare equal; a one-byte payload change
   must reach the production comparison and be rejected. No repository mutation. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $PSScriptRoot 'PayloadManifestFixture'
$fixtureRoot = Join-Path $OwnedOutputRoot ('payload-comparison-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
$firstRoot = Join-Path $fixtureRoot 'first-root'
$secondRoot = Join-Path $fixtureRoot 'second-root'
Copy-Item -LiteralPath $fixture -Destination $firstRoot -Recurse
Copy-Item -LiteralPath $fixture -Destination $secondRoot -Recurse
$producer = Join-Path $repositoryRoot 'scripts/New-PayloadManifest.ps1'
$comparison = Join-Path $repositoryRoot 'scripts/Test-PayloadManifestEquality.ps1'
$firstManifest = Join-Path $fixtureRoot 'first.json'
$secondManifest = Join-Path $fixtureRoot 'second.json'
$changedManifest = Join-Path $fixtureRoot 'changed.json'
& $producer -PayloadRoot $firstRoot -OutputPath $firstManifest
& $producer -PayloadRoot $secondRoot -OutputPath $secondManifest
& $comparison -FirstManifestPath $firstManifest -SecondManifestPath $secondManifest

$changedFile = Join-Path $secondRoot 'Z.txt'
$before = [IO.File]::ReadAllBytes($changedFile)
if ($before.Length -ne 6 -or $before[0] -ne 104) { throw 'One-byte mutation fixture precondition is unproved.' }
$mutation = [IO.FileStream]::new($changedFile,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $mutation.WriteByte(72); $mutation.Flush($true) }
finally { $mutation.Dispose() }
$after = [IO.File]::ReadAllBytes($changedFile)
if ($after.Length -ne $before.Length -or $after[0] -ne 72) { throw 'Intended same-length one-byte mutation was not observed.' }
for ($index=1; $index -lt $before.Length; ++$index) {
    if ($after[$index] -ne $before[$index]) { throw 'Fixture changed more than one byte.' }
}
& $producer -PayloadRoot $secondRoot -OutputPath $changedManifest
$message = $null
try { & $comparison -FirstManifestPath $firstManifest -SecondManifestPath $changedManifest }
catch { $message = $_.Exception.Message }
if ($message -cne 'Canonical payload manifests differ.') { throw "One-byte mutation was not rejected by the production comparison: $message" }
Write-Output 'OBSERVED NEGATIVE CONTROL: Canonical payload manifests differ after exactly one same-length byte changes.'
Write-Output 'PASS: distinct-root equality and actual one-byte mismatch sensitivity.'
