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
$powershell=(Get-Process -Id $PID).Path
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'IndependentSourceBuildPair.psm1') -Force
# Bind the existing native invoker into the callback's closure. The workspace
# module owns only checkout lifecycle, not compilation or a second process API.
$invokeNativeProcess=Get-Command Invoke-ArtifactToolProcess -Module ArtifactToolProcess -ErrorAction Stop
$builds=[Collections.Generic.List[object]]::new()
$buildAction={
    param($clone,$name)
    $evidence=Join-Path $candidate $name
    [void][IO.Directory]::CreateDirectory($evidence)
    $packages=[Collections.Generic.List[string]]::new()
    foreach ($architecture in @('x86','x64','ARM64')) {
        Write-Host "Independent build comparison: $name Release $architecture (two MSBuild workers, sequential)."
        $packageOutput=Join-Path $clone "artifacts/package-comparison-packages/$architecture"
        & $invokeNativeProcess -Executable $powershell -WorkingDirectory $clone -Arguments @('-NoProfile','-File',(Join-Path $clone 'scripts/Invoke-Build.ps1'),
            '-Configuration','Release','-Architecture',$architecture,'-MaximumBuildProcessCount','2',
            '-BuildEntryPoint','PackagedApplication','-UnsignedPackageOutputDirectory',$packageOutput,
            '-PrepareStoreSubmissionPackage') -LogDirectory $evidence -LogName "build-$architecture" -TimeoutSeconds 2400 | Out-Null
        # MSBuild owns the evaluated output name. Ordinary and upload components
        # can coexist; choosing the first glob match would bypass serial packing.
        $selection=@(Get-Content -LiteralPath (Join-Path $packageOutput store-submission-package-path.txt))
        if ($selection.Count -ne 1 -or [string]::IsNullOrWhiteSpace($selection[0]) -or
            -not [IO.Path]::IsPathFullyQualified($selection[0])) {throw 'Native Store package selection must be one absolute path.'}
        $selectedPackage=$selection[0]
        $selectedRelativePath=[IO.Path]::GetRelativePath($packageOutput,$selectedPackage)
        if ([IO.Path]::IsPathRooted($selectedRelativePath) -or $selectedRelativePath -eq '..' -or
            $selectedRelativePath.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal) -or
            -not [IO.File]::Exists($selectedPackage) -or
            [IO.Path]::GetExtension($selectedPackage) -cne '.msix') {throw 'Native Store package selection must identify an existing MSIX within its owned output.'}
        $ancestor=$selectedPackage
        while (-not [string]::IsNullOrEmpty($ancestor)) {
            if (([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Native Store package selection refuses redirected paths.'}
            $ancestor=[IO.Path]::GetDirectoryName($ancestor)
        }
        $packages.Add($selectedPackage)
        & $invokeNativeProcess -Executable $powershell -WorkingDirectory $clone -Arguments @('-NoProfile','-File',(Join-Path $clone 'scripts/New-BuildProvenance.ps1'),
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
}.GetNewClosure()
$workspacePair=Invoke-IndependentSourceBuildPair -SourceRepositoryRoot $source -SourceCommit $SourceCommit `
    -WorkingDirectory (Join-Path $working builds) -LogDirectory (Join-Path $candidate checkout-logs) -BuildAction $buildAction
$comparison=& (Join-Path $PSScriptRoot 'Compare-JpgSpinnerBundlePayload.ps1') -FirstBundlePath (Join-Path $candidate first/JpgSpinner.msixbundle) `
    -SecondBundlePath (Join-Path $candidate second/JpgSpinner.msixbundle) -WorkingDirectory (Join-Path $candidate comparison)
$record=@{
    schemaVersion=1;sourceCommit=$SourceCommit;sourceSignatureVerified=$true
    comparisonSatisfied=$comparison.comparisonSatisfied;payloadsByteIdentical=$comparison.payloadsByteIdentical
    containersByteIdentical=$comparison.containersByteIdentical;comparisonReportRelativePath='comparison/package-payload-comparison.json'
    # Record this actual environment input, not an assertion of path-insensitive
    # output. Physical checkout trees are independent and retained separately.
    buildEnvironment=@{canonicalSourceBuildPath=$workspacePair.Builds[0].BuildPath;pathIsControlled=$true;retainedCheckouts=$workspacePair.Builds}
    builds=$builds.ToArray();releaseApproval='pending'
}
[IO.File]::WriteAllText((Join-Path $candidate independent-package-build-comparison.json),($record | ConvertTo-Json -Depth 5)+"`n",[Text.UTF8Encoding]::new($false))
[IO.Directory]::Move($candidate,$output)
Write-Output $output
