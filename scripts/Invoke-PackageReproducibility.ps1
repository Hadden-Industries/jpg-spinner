<# Build the exact signed source twice in independent clean native checkouts.
   Payload equality, container equality and release approval are distinct claims.
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
if ($LASTEXITCODE -ne 0 -or $head -cne $SourceCommit) {throw 'Reproducibility requires the selected source commit to be the checkout HEAD.'}
if ((Test-Path -LiteralPath $working) -or (Test-Path -LiteralPath $output)) {throw 'Reproducibility output and work must be fresh and absent.'}
function Test-Within([string]$Root,[string]$Path) {
    $relative=[IO.Path]::GetRelativePath($Root,$Path)
    return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
        -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
}
foreach ($destination in @($working,$output)) {
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {throw 'Reproducibility destination parents must exist.'}
    if ((Test-Within $source $destination) -or (Test-Within $destination $source)) {throw 'Reproducibility destinations must be independent of the source checkout.'}
}
if ((Test-Within $working $output) -or (Test-Within $output $working) -or
    -not [string]::Equals([IO.Path]::GetPathRoot($working),[IO.Path]::GetPathRoot($output),[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Reproducibility destinations must be disjoint on the same volume.'
}
foreach ($path in @($source,$working,$output)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Reproducibility refuses redirected roots or destination ancestors.'}
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
$status=@(& git -C $source status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0 -or $status.Count -ne 0) {throw 'Reproducibility requires a clean selected source checkout.'}
# Verify the chosen source's existing signature; never create/alter a source
# commit or substitute a branch that can advance while the two builds run.
& git -C $source verify-commit $SourceCommit
if ($LASTEXITCODE -ne 0) {throw 'Reproducibility source commit signature verification failed.'}
[void][IO.Directory]::CreateDirectory($working)
$candidate=Join-Path $working evidence
[void][IO.Directory]::CreateDirectory($candidate)
$git=(Get-Command git -CommandType Application -ErrorAction Stop).Source
$powershell=(Get-Process -Id $PID).Path
function Invoke-OwnedTool([string]$Executable,[string]$Directory,[string[]]$Arguments,[string]$LogPath,[int]$TimeoutMilliseconds) {
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$Executable;$start.WorkingDirectory=$Directory
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach ($argument in $Arguments) {$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($TimeoutMilliseconds)
        if ($timedOut) {
            $process.Kill($true)
            if (-not $process.WaitForExit(5000)) {throw 'Reproducibility child termination remains unresolved.'}
        }
        $text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText($LogPath,$text,[Text.UTF8Encoding]::new($false))
        if ($timedOut) {throw "Reproducibility child exceeded its bounded deadline; retain its log: $LogPath"}
        if ($process.ExitCode -ne 0) {throw "Native reproducibility step failed; retain its log: $LogPath"}
    } finally {$process.Dispose()}
}
$builds=[Collections.Generic.List[object]]::new()
foreach ($name in @('first','second')) {
    $clone=Join-Path $working $name
    $evidence=Join-Path $candidate $name
    [void][IO.Directory]::CreateDirectory($evidence)
    # --no-local prevents shared object stores/hardlinks or alternates. Each
    # clone starts with no intermediates or installed native dependency tree.
    Invoke-OwnedTool $git $source @('clone','--no-local','--no-hardlinks','--no-checkout','--',$source,$clone) (Join-Path $evidence clone.log) 120000
    Invoke-OwnedTool $git $clone @('checkout','--detach',$SourceCommit) (Join-Path $evidence checkout.log) 120000
    $actual=& $git -C $clone rev-parse --verify HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -cne $SourceCommit) {throw 'Independent checkout selected a different commit.'}
    $packages=[Collections.Generic.List[string]]::new()
    foreach ($architecture in @('x86','x64','ARM64')) {
        Write-Host "Reproducibility: $name Release $architecture (two MSBuild workers, sequential)."
        $packageOutput=Join-Path $clone "artifacts/reproducibility-packages/$architecture"
        Invoke-OwnedTool $powershell $clone @('-NoProfile','-File',(Join-Path $clone 'scripts/Invoke-Build.ps1'),
            '-Configuration','Release','-Architecture',$architecture,'-MaximumBuildProcessCount','2',
            '-BuildEntryPoint','PackagedApplication','-UnsignedPackageOutputDirectory',$packageOutput) (Join-Path $evidence "build-$architecture.log") 2400000
        $produced=@(Get-ChildItem -LiteralPath $packageOutput -File -Recurse -Filter 'JpgSpinner.App_*.msix')
        if ($produced.Count -ne 1) {throw 'Independent build did not emit exactly one architecture package.'}
        $packages.Add($produced[0].FullName)
        Invoke-OwnedTool $powershell $clone @('-NoProfile','-File',(Join-Path $clone 'scripts/New-BuildProvenance.ps1'),
            '-Configuration','Release','-Architecture',$architecture,'-RequireCleanSourceTree',
            '-OutputPath',(Join-Path $evidence "provenance-$architecture.json")) (Join-Path $evidence "provenance-$architecture.log") 120000
    }
    $bundle=Join-Path $evidence 'JpgSpinner.msixbundle'
    # Pass the typed array directly to the existing repository producer. Native
    # PowerShell -File cannot carry an array parameter as separate arguments;
    # do not invent an argument grammar or interpolate paths into shell code.
    & (Join-Path $clone 'scripts/New-MsixBundle.ps1') -PackagePaths $packages.ToArray() -OutputPath $bundle `
        -WorkingDirectory (Join-Path $clone artifacts/reproducibility-bundle-staging) *> (Join-Path $evidence bundle.log)
    $inspection=Join-Path $clone 'artifacts/reproducibility-inspection'
    Invoke-OwnedTool $powershell $clone @('-NoProfile','-File',(Join-Path $clone 'scripts/Expand-JpgSpinnerBundle.ps1'),
        '-BundlePath',$bundle,'-InspectionDirectory',$inspection) (Join-Path $evidence inspection.log) 120000
    & (Join-Path $clone 'scripts/New-PayloadManifest.ps1') -PayloadRoot (Join-Path $inspection payloads) -OutputPath (Join-Path $evidence payload.json) | Out-Null
    $builds.Add(@{name=$name;unsignedBundleSha256=(Get-FileHash -LiteralPath $bundle).Hash.ToLowerInvariant();payloadManifestSha256=(Get-FileHash -LiteralPath (Join-Path $evidence payload.json)).Hash.ToLowerInvariant()})
}
& (Join-Path $PSScriptRoot 'Test-PayloadManifestEquality.ps1') -FirstManifestPath (Join-Path $candidate first/payload.json) -SecondManifestPath (Join-Path $candidate second/payload.json) | Out-Null
$record=@{schemaVersion=1;sourceCommit=$SourceCommit;sourceSignatureVerified=$true;payloadsEqual=$true;unsignedContainersEqual=($builds[0].unsignedBundleSha256 -ceq $builds[1].unsignedBundleSha256);builds=$builds.ToArray();releaseApproval='pending'}
[IO.File]::WriteAllText((Join-Path $candidate reproducibility.json),($record | ConvertTo-Json -Depth 5)+"`n",[Text.UTF8Encoding]::new($false))
[IO.Directory]::Move($candidate,$output)
Write-Output $output
