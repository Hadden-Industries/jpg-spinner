<# Exercise the real instrumented scanner and native libFuzzer corpus merge.
   A new owned run must preserve its full corpus and publish a nonempty native
   coverage-preserving subset; a generation-only fuzz pass is insufficient. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fuzzScript = Join-Path $repositoryRoot 'scripts/Invoke-ScannerFuzz.ps1'

# Reject reuse before toolchain resolution or a native build. A failed run must
# not turn into permission to replace its existing evidence on the next call.
$occupiedDirectory = Join-Path $OwnedOutputRoot ('scanner-existing-evidence-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($occupiedDirectory)
$sentinel = Join-Path $occupiedDirectory 'retained-result.txt'
[IO.File]::WriteAllText($sentinel, 'independently retained prior result')
$powerShellPath = (Get-Process -Id $PID).Path
$refusal = & $powerShellPath -NoProfile -File $fuzzScript -DurationSeconds 1 -RunDirectory $occupiedDirectory 2>&1
$refusalExitCode = $LASTEXITCODE
if ($refusalExitCode -eq 0 -or ($refusal -join "`n") -notmatch 'RunDirectory must be empty') {
    throw 'The fuzz driver did not refuse an occupied run directory before building.'
}
if ([IO.File]::ReadAllText($sentinel) -cne 'independently retained prior result' -or
    @(Get-ChildItem -LiteralPath $occupiedDirectory -Force).Count -ne 1) {
    throw 'Run-directory refusal changed existing evidence.'
}
# Independent redundant inputs distinguish minimization from copying every
# corpus file. Native fuzzing retains those input files, even after deduping
# its in-memory corpus. Never modify the historical development corpus.
$fixture = Join-Path $OwnedOutputRoot ('scanner-merge-fixture-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$seedPath = Join-Path $repositoryRoot 'fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/corpus/baseline.hex'
$seed = [Convert]::FromHexString(([IO.File]::ReadAllText($seedPath)).Trim())
foreach ($name in @('same-a','same-b')) { [IO.File]::WriteAllBytes((Join-Path $fixture $name), $seed) }
# Use an explicitly reserved child of the caller-owned working group. Looking
# for a new UUID in a shared product root races other runs and makes cleanup
# ownership indirect; the actual fuzz driver must honor this exact location.
$runDirectory = Join-Path $OwnedOutputRoot ('scanner-merge-run-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($runDirectory)
& $fuzzScript -DurationSeconds 1 -AdditionalCorpusDirectory $fixture -RunDirectory $runDirectory
$preservedInputs = @(Get-ChildItem -LiteralPath $fixture -File)
if ($preservedInputs.Count -ne 2) { throw 'Fuzz qualification changed the original synthetic input corpus.' }
foreach ($file in $preservedInputs) {
    if (-not [Linq.Enumerable]::SequenceEqual([byte[]][IO.File]::ReadAllBytes($file.FullName), [byte[]]$seed)) {
        throw 'Fuzz qualification changed an original synthetic corpus input.'
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $runDirectory 'minimized') -PathType Container)) {
    throw 'SCANNER_CORPUS_MINIMIZATION_MISSING: a fresh run must contain the native minimized corpus.'
}
$source = @(Get-ChildItem -LiteralPath (Join-Path $runDirectory 'corpus') -File)
$minimized = @(Get-ChildItem -LiteralPath (Join-Path $runDirectory 'minimized') -File)
if ($source.Count -eq 0 -or $minimized.Count -eq 0 -or $minimized.Count -gt $source.Count) {
    throw 'Native corpus minimization produced an empty or larger corpus.'
}
$sourceHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($file in $source) {
    [void]$sourceHashes.Add((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)
}
if ($sourceHashes.Count -ge $source.Count) {
    throw 'The deliberate duplicate-corpus fixture did not reach the real target.'
}
if ($minimized.Count -ge $source.Count) { throw 'Redundant corpus entries were copied rather than minimized.' }
foreach ($file in $minimized) {
    if (-not $sourceHashes.Contains((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)) {
        throw 'Native minimized corpus contains bytes outside the preserved full corpus.'
    }
}
foreach ($phase in @('smoke','fuzz','replay','merge','merge-audit')) {
    if (-not (Test-Path -LiteralPath (Join-Path $runDirectory "$phase.stderr.log") -PathType Leaf)) {
        throw "Missing retained native $phase diagnostics."
    }
}
Write-Output (
    'PASS: native instrumented fuzzing, full-corpus replay, minimized subset and no-additional-coverage merge audit.'
)
