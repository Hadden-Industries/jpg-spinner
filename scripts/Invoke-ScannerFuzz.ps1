<# Run the instrumented scanner, then retain a native coverage-preserving corpus.
   Each invocation owns a fresh UUID directory. Historical corpus/reproducers
   remain untouched; libFuzzer owns feature selection, not a custom minimizer. #>
[CmdletBinding()]
param(
    [ValidateRange(1, 86400)][int]$DurationSeconds = 60,
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    # An explicit synthetic corpus supports retained-reproducer qualification
    # without mutating it. Empty selects the earlier task-owned development corpus.
    [string]$AdditionalCorpusDirectory = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $repositoryRoot
$fuzzDirectory = Join-Path $repositoryRoot 'artifacts/fuzz/scanner'
$runDirectory = Join-Path $fuzzDirectory ([guid]::NewGuid().ToString('N'))
$corpusDirectory = Join-Path $runDirectory 'corpus'
$minimizedDirectory = Join-Path $runDirectory 'minimized'
[void][IO.Directory]::CreateDirectory($corpusDirectory)

# Keep the previous task-owned synthetic development corpus as read-only input.
# Copy into this run instead of fuzzing or replacing earlier evidence in place.
$priorCorpus = if ([string]::IsNullOrEmpty($AdditionalCorpusDirectory)) {
    Join-Path $fuzzDirectory 'corpus'
} else {
    $resolvedCorpus = [IO.Path]::GetFullPath($AdditionalCorpusDirectory)
    if (-not [IO.Directory]::Exists($resolvedCorpus)) { throw 'AdditionalCorpusDirectory must exist.' }
    $resolvedCorpus
}
if (Test-Path -LiteralPath $priorCorpus) {
    if (([IO.File]::GetAttributes($priorCorpus) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Previous synthetic corpus root must not be a reparse point.'
    }
    foreach ($entry in Get-ChildItem -LiteralPath $priorCorpus -Force) {
        if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $entry.Length -gt 1048576) {
            throw 'Previous synthetic corpus must contain only ordinary files of at most 1 MiB.'
        }
        [IO.File]::Copy($entry.FullName, (Join-Path $corpusDirectory $entry.Name), $false)
    }
}
# Reviewable authored hex inputs are never user photographs. A seed with the
# same name in the copied corpus must have identical bytes, not be overwritten.
$seedDirectory = Join-Path $repositoryRoot 'fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/corpus'
foreach ($seed in Get-ChildItem -LiteralPath $seedDirectory -Filter '*.hex') {
    $bytes = [Convert]::FromHexString(([IO.File]::ReadAllText($seed.FullName)).Trim())
    $seedPath = Join-Path $corpusDirectory $seed.BaseName
    if (Test-Path -LiteralPath $seedPath) {
        if (-not [Linq.Enumerable]::SequenceEqual([byte[]][IO.File]::ReadAllBytes($seedPath), [byte[]]$bytes)) {
            throw 'Copied corpus conflicts with an authored seed.'
        }
    } else { [IO.File]::WriteAllBytes($seedPath, $bytes) }
}

# Invoke-FuzzTarget executes only a repository-built target, with native ASan
# runtime resolution, an owned-process deadline and separate retained streams.
# Nonzero exit, timeout or a failed kill remains a failure; no retries occur.
function Invoke-FuzzTarget {
    param([string]$ProjectName, [string[]]$Arguments, [int]$TimeoutSeconds, [string]$Phase)
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = Join-Path $repositoryRoot "artifacts/bin/x64/Fuzz/$ProjectName/$ProjectName.exe"
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Environment['PATH'] = (Split-Path -Parent $toolchain.targetTools.x64.compilerExecutablePath) +
        [IO.Path]::PathSeparator + $startInfo.Environment['PATH']
    foreach ($argument in $Arguments) { [void]$startInfo.ArgumentList.Add($argument) }
    $stdout = [IO.File]::Open((Join-Path $runDirectory "$Phase.stdout.log"), [IO.FileMode]::CreateNew)
    $stderr = [IO.File]::Open((Join-Path $runDirectory "$Phase.stderr.log"), [IO.FileMode]::CreateNew)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $stdoutCopy = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $stderrCopy = $process.StandardError.BaseStream.CopyToAsync($stderr)
        $exited = $process.WaitForExit($TimeoutSeconds * 1000)
        if (-not $exited) {
            $process.Kill($true)
            if (-not $process.WaitForExit(5000)) { throw "Owned $Phase target did not terminate after its deadline." }
        }
        [void]$stdoutCopy.GetAwaiter().GetResult()
        [void]$stderrCopy.GetAwaiter().GetResult()
        if (-not $exited) { throw "Fuzz $Phase exceeded its $TimeoutSeconds-second process budget." }
        if ($process.ExitCode -ne 0) { throw "Fuzz $Phase failed with native exit $($process.ExitCode); retain $runDirectory." }
    }
    finally { $process.Dispose(); $stdout.Dispose(); $stderr.Dispose() }
}

foreach ($projectName in @('JpgSpinner.FuzzToolchain.Smoke', 'JpgSpinner.JpegSegmentScanner.Fuzz')) {
    $projectPath = Join-Path $repositoryRoot "fuzz/$projectName/$projectName.vcxproj"
    # Native vcpkg supports mapping custom build configurations explicitly.
    # Fuzz uses the Release CRT; this changes no locked compiler/SDK selection.
    & $toolchain.msBuildExecutablePath $projectPath -nologo -noAutoResponse -maxcpucount:2 `
        -p:Configuration=Fuzz -p:Platform=x64 -p:VcpkgConfiguration=Release -verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "Fuzz build failed: $projectName ($LASTEXITCODE)" }
}
Invoke-FuzzTarget -ProjectName 'JpgSpinner.FuzzToolchain.Smoke' -Arguments @('-runs=100', '-seed=20260930') -TimeoutSeconds 30 -Phase 'smoke'

$scanner = 'JpgSpinner.JpegSegmentScanner.Fuzz'
$limits = @('-timeout=10', '-rss_limit_mb=2048', '-max_len=1048576', '-seed=20260930', "-artifact_prefix=$runDirectory/")
Invoke-FuzzTarget -ProjectName $scanner -Arguments (@($corpusDirectory, "-max_total_time=$DurationSeconds") + $limits) -TimeoutSeconds ($DurationSeconds + 30) -Phase 'fuzz'

# Merge can recover from crashing inputs internally. Replay the full corpus
# first so a crashing/timeout/ASan input cannot be silently filtered as a pass.
Invoke-FuzzTarget -ProjectName $scanner -Arguments (@($corpusDirectory, '-runs=0') + $limits) -TimeoutSeconds 60 -Phase 'replay'
[void][IO.Directory]::CreateDirectory($minimizedDirectory)
Invoke-FuzzTarget -ProjectName $scanner -Arguments (@('-merge=1', $minimizedDirectory, $corpusDirectory, "-merge_control_file=$runDirectory/merge.control") + $limits) -TimeoutSeconds 60 -Phase 'merge'
$minimizedBeforeAudit = @(Get-ChildItem -LiteralPath $minimizedDirectory -File | ForEach-Object Name | Sort-Object)
if ($minimizedBeforeAudit.Count -eq 0) { throw 'Native corpus merge did not retain any coverage-bearing input.' }

# Native merge itself is the coverage oracle. Feeding the full corpus into the
# already-minimized corpus must add no files; do not scrape feature-count prose.
Invoke-FuzzTarget -ProjectName $scanner -Arguments (@('-merge=1', $minimizedDirectory, $corpusDirectory, "-merge_control_file=$runDirectory/merge-audit.control") + $limits) -TimeoutSeconds 60 -Phase 'merge-audit'
$minimizedAfterAudit = @(Get-ChildItem -LiteralPath $minimizedDirectory -File | ForEach-Object Name | Sort-Object)
if (@(Compare-Object $minimizedBeforeAudit $minimizedAfterAudit).Count -ne 0) {
    throw 'Native merge audit found additional coverage; the proposed minimized corpus is not qualified.'
}
Write-Output "PASS: bounded native scanner fuzzing, replay, corpus merge and coverage audit; retained run $runDirectory."
