<# Exercise the real instrumented scanner and native libFuzzer corpus merge.
   A new owned run must preserve its full corpus and publish a nonempty native
   coverage-preserving subset; a generation-only fuzz pass is insufficient. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fuzzRoot = Join-Path $repositoryRoot 'artifacts/fuzz/scanner'
# Independent redundant inputs distinguish minimization from copying every
# corpus file. Native fuzzing retains those input files, even after deduping
# its in-memory corpus. Never modify the historical development corpus.
$fixture = Join-Path $OwnedOutputRoot ('scanner-merge-fixture-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$seed = [Convert]::FromHexString(([IO.File]::ReadAllText((Join-Path $repositoryRoot 'fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/corpus/baseline.hex'))).Trim())
foreach ($name in @('same-a','same-b')) { [IO.File]::WriteAllBytes((Join-Path $fixture $name), $seed) }
$existingRuns = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
if (Test-Path -LiteralPath $fuzzRoot) {
    foreach ($directory in Get-ChildItem -LiteralPath $fuzzRoot -Directory) { [void]$existingRuns.Add($directory.Name) }
}
& (Join-Path $repositoryRoot 'scripts/Invoke-ScannerFuzz.ps1') -DurationSeconds 1 -AdditionalCorpusDirectory $fixture
$preservedInputs = @(Get-ChildItem -LiteralPath $fixture -File)
if ($preservedInputs.Count -ne 2) { throw 'Fuzz qualification changed the original synthetic input corpus.' }
foreach ($file in $preservedInputs) {
    if (-not [Linq.Enumerable]::SequenceEqual([byte[]][IO.File]::ReadAllBytes($file.FullName), [byte[]]$seed)) {
        throw 'Fuzz qualification changed an original synthetic corpus input.'
    }
}
$newRuns = @(Get-ChildItem -LiteralPath $fuzzRoot -Directory | Where-Object { -not $existingRuns.Contains($_.Name) })
if ($newRuns.Count -ne 1 -or -not (Test-Path -LiteralPath (Join-Path $newRuns[0].FullName 'minimized') -PathType Container)) {
    throw 'SCANNER_CORPUS_MINIMIZATION_MISSING: a fresh run must contain the native minimized corpus.'
}
$source = @(Get-ChildItem -LiteralPath (Join-Path $newRuns[0].FullName 'corpus') -File)
$minimized = @(Get-ChildItem -LiteralPath (Join-Path $newRuns[0].FullName 'minimized') -File)
if ($source.Count -eq 0 -or $minimized.Count -eq 0 -or $minimized.Count -gt $source.Count) { throw 'Native corpus minimization produced an empty or larger corpus.' }
$sourceHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($file in $source) { [void]$sourceHashes.Add((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) }
if ($sourceHashes.Count -ge $source.Count) { throw 'The deliberate duplicate-corpus fixture did not reach the real target.' }
if ($minimized.Count -ge $source.Count) { throw 'Redundant corpus entries were copied rather than minimized.' }
foreach ($file in $minimized) {
    if (-not $sourceHashes.Contains((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)) { throw 'Native minimized corpus contains bytes outside the preserved full corpus.' }
}
foreach ($phase in @('smoke','fuzz','replay','merge','merge-audit')) {
    if (-not (Test-Path -LiteralPath (Join-Path $newRuns[0].FullName "$phase.stderr.log") -PathType Leaf)) { throw "Missing retained native $phase diagnostics." }
}
Write-Output 'PASS: native instrumented fuzzing, full-corpus replay, minimized subset and no-additional-coverage merge audit.'
