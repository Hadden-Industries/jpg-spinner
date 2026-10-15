<# Build the exact signed source twice in independent clean native checkouts.
   Closed payload comparison, actual byte identity and release approval are
   distinct claims. Only the accepted app WinMD identity policy permits variance.
   No signing, deployment, desktop access, publishing or automatic cleanup. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$SourceRepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$source=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SourceRepositoryRoot)
$working=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
$output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$head=& git -C $source rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0 -or $head -cne $SourceCommit) {throw 'Independent build comparison requires the selected source commit to be the checkout HEAD.'}
if ((Test-Path -LiteralPath $working) -or (Test-Path -LiteralPath $output)) {throw 'Independent build comparison output and work must be fresh and absent.'}
function Test-Within([string]$Root,[string]$Path) {
    $relative=[IO.Path]::GetRelativePath($Root,$Path)
    return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
        -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
}
foreach ($destination in @($working,$output)) {
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {throw 'Independent build comparison destination parents must exist.'}
    if ((Test-Within $source $destination) -or (Test-Within $destination $source)) {throw 'Independent build comparison destinations must be independent of the source checkout.'}
}
if ((Test-Within $working $output) -or (Test-Within $output $working) -or
    -not [string]::Equals([IO.Path]::GetPathRoot($working),[IO.Path]::GetPathRoot($output),[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Independent build comparison destinations must be disjoint on the same volume.'
}
foreach ($path in @($source,$working,$output)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Independent build comparison refuses redirected roots or destination ancestors.'}
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
$status=@(& git -C $source status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0 -or $status.Count -ne 0) {throw 'Independent build comparison requires a clean selected source checkout.'}
# Verify the chosen source's existing signature; never create/alter a source
# commit or substitute a branch that can advance while the two builds run.
& git -C $source verify-commit $SourceCommit
if ($LASTEXITCODE -ne 0) {throw 'Independent build comparison source commit signature verification failed.'}
[void][IO.Directory]::CreateDirectory($working)
$candidate=Join-Path $working evidence
[void][IO.Directory]::CreateDirectory($candidate)
# PowerShell may enumerate multiple Application matches when CommandType is
# constrained. Match ordinary command resolution's first executable, not a
# space-joined list of PATH candidates passed to ProcessStartInfo.FileName.
$git=(Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$powershell=(Get-Process -Id $PID).Path
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force
$builds=[Collections.Generic.List[object]]::new()
foreach ($name in @('first','second')) {
    $clone=Join-Path $working $name
    $evidence=Join-Path $candidate $name
    [void][IO.Directory]::CreateDirectory($evidence)
    # --no-local prevents shared object stores/hardlinks or alternates. Each
    # clone starts with no intermediates or installed native dependency tree.
    Invoke-ArtifactToolProcess -Executable $git -WorkingDirectory $source -Arguments @('clone','--no-local','--no-hardlinks','--no-checkout','--',$source,$clone) -LogDirectory $evidence -LogName clone -TimeoutSeconds 120 | Out-Null
    Invoke-ArtifactToolProcess -Executable $git -WorkingDirectory $clone -Arguments @('checkout','--detach',$SourceCommit) -LogDirectory $evidence -LogName checkout -TimeoutSeconds 120 | Out-Null
    $actual=& $git -C $clone rev-parse --verify HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -cne $SourceCommit) {throw 'Independent checkout selected a different commit.'}
    $packages=[Collections.Generic.List[string]]::new()
    foreach ($architecture in @('x86','x64','ARM64')) {
        Write-Host "Independent build comparison: $name Release $architecture (two MSBuild workers, sequential)."
        $packageOutput=Join-Path $clone "artifacts/package-comparison-packages/$architecture"
        Invoke-ArtifactToolProcess -Executable $powershell -WorkingDirectory $clone -Arguments @('-NoProfile','-File',(Join-Path $clone 'scripts/Invoke-Build.ps1'),
            '-Configuration','Release','-Architecture',$architecture,'-MaximumBuildProcessCount','2',
            '-BuildEntryPoint','PackagedApplication','-UnsignedPackageOutputDirectory',$packageOutput) -LogDirectory $evidence -LogName "build-$architecture" -TimeoutSeconds 2400 | Out-Null
        $produced=@(Get-ChildItem -LiteralPath $packageOutput -File -Recurse -Filter 'JpgSpinner.App_*.msix')
        if ($produced.Count -ne 1) {throw 'Independent build did not emit exactly one architecture package.'}
        $packages.Add($produced[0].FullName)
        Invoke-ArtifactToolProcess -Executable $powershell -WorkingDirectory $clone -Arguments @('-NoProfile','-File',(Join-Path $clone 'scripts/New-BuildProvenance.ps1'),
            '-Configuration','Release','-Architecture',$architecture,'-RequireCleanSourceTree',
            '-OutputPath',(Join-Path $evidence "provenance-$architecture.json")) -LogDirectory $evidence -LogName "provenance-$architecture" -TimeoutSeconds 120 | Out-Null
    }
    $bundle=Join-Path $evidence 'JpgSpinner.msixbundle'
    # Pass the typed array directly to the existing repository producer. Native
    # PowerShell -File cannot carry an array parameter as separate arguments;
    # do not invent an argument grammar or interpolate paths into shell code.
    & (Join-Path $clone 'scripts/New-MsixBundle.ps1') -PackagePaths $packages.ToArray() -OutputPath $bundle `
        -WorkingDirectory (Join-Path $clone artifacts/package-comparison-bundle-staging) *> (Join-Path $evidence bundle.log)
    $builds.Add(@{name=$name;unsignedBundleSha256=(Get-FileHash -LiteralPath $bundle).Hash.ToLowerInvariant()})
}
$comparison=& (Join-Path $PSScriptRoot 'Compare-JpgSpinnerBundlePayload.ps1') -FirstBundlePath (Join-Path $candidate first/JpgSpinner.msixbundle) `
    -SecondBundlePath (Join-Path $candidate second/JpgSpinner.msixbundle) -WorkingDirectory (Join-Path $candidate comparison)
$record=@{
    schemaVersion=1;sourceCommit=$SourceCommit;sourceSignatureVerified=$true
    comparisonSatisfied=$comparison.comparisonSatisfied;payloadsByteIdentical=$comparison.payloadsByteIdentical
    containersByteIdentical=$comparison.containersByteIdentical;comparisonReportRelativePath='comparison/package-payload-comparison.json'
    builds=$builds.ToArray();releaseApproval='pending'
}
[IO.File]::WriteAllText((Join-Path $candidate independent-package-build-comparison.json),($record | ConvertTo-Json -Depth 5)+"`n",[Text.UTF8Encoding]::new($false))
[IO.Directory]::Move($candidate,$output)
Write-Output $output
