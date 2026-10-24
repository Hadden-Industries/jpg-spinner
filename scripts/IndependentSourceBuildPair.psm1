<# Own two native Git checkout/build lifecycles. The caller authenticates the
   selected source and supplies its build action, which must finish its owned
   children before returning. No signing, release, cleanup or artifact rewriting.
   Native Git owns objects/checkout semantics; Directory.Move retains completed
   task-owned trees without reusing intermediates for the next independent clone. #>
Set-StrictMode -Version Latest
# Reuse the current dependency instance. Forcing a reload from this private
# scope removes the caller's exported invoker and breaks its build callback.
Import-Module (Join-Path $PSScriptRoot ArtifactToolProcess.psm1)

function Invoke-IndependentSourceBuildPair {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$SourceRepositoryRoot,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [Parameter(Mandatory)][scriptblock]$BuildAction
    )
    $ErrorActionPreference = 'Stop'
    $source = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SourceRepositoryRoot)
    $working = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
    $logs = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($LogDirectory)
    function Test-Within([string]$Root, [string]$Path) {
        $relative = [IO.Path]::GetRelativePath($Root, $Path)
        return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
            -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)", [StringComparison]::Ordinal)
    }
    function Assert-OrdinaryPathAncestors([string]$Path) {
        $ancestor = $Path
        while (-not [string]::IsNullOrEmpty($ancestor)) {
            if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Independent source builds refuse redirected roots.' }
            $ancestor = [IO.Path]::GetDirectoryName($ancestor)
        }
    }
    foreach ($path in @($source, $working, $logs)) { Assert-OrdinaryPathAncestors $path }
    if (-not [IO.Directory]::Exists($source)) { throw 'Independent source repository is absent.' }
    foreach ($destination in @($working, $logs)) {
        if ((Test-Path -LiteralPath $destination) -or -not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) { throw 'Independent source destinations must be fresh with existing parents.' }
        if ((Test-Within $source $destination) -or (Test-Within $destination $source)) { throw 'Independent source destinations overlap the source.' }
    }
    if ((Test-Within $working $logs) -or (Test-Within $logs $working)) { throw 'Independent checkout and log directories must be disjoint.' }
    [void][IO.Directory]::CreateDirectory($working)
    [void][IO.Directory]::CreateDirectory($logs)
    $retained = Join-Path $working retained
    [void][IO.Directory]::CreateDirectory($retained)
    $git = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $builds = [Collections.Generic.List[object]]::new()
    foreach ($name in @('first', 'second')) {
        # Absolute input paths are part of the specified native build
        # environment, not an allowed binary variance. Each pass creates a new
        # independent directory here after retaining its predecessor elsewhere.
        $checkout = Join-Path $working source
        $buildLogs = Join-Path $logs $name
        [void][IO.Directory]::CreateDirectory($buildLogs)
        Invoke-ArtifactToolProcess -Executable $git -WorkingDirectory $source -LogDirectory $buildLogs -LogName clone `
            -Arguments @('clone', '--no-local', '--no-hardlinks', '--no-checkout', '--', $source, $checkout) | Out-Null
        Invoke-ArtifactToolProcess -Executable $git -WorkingDirectory $checkout -LogDirectory $buildLogs -LogName checkout `
            -Arguments @('checkout', '--detach', $SourceCommit) | Out-Null
        $head = & $git -C $checkout rev-parse --verify HEAD
        if ($LASTEXITCODE -ne 0 -or $head -cne $SourceCommit) { throw 'Independent clone selected a different source commit.' }
        & $BuildAction $checkout $name | Out-Null
        $status = @(& $git -C $checkout status --porcelain --untracked-files=normal)
        if ($LASTEXITCODE -ne 0 -or $status.Count -ne 0) { throw 'Build action changed selected source inputs.' }
        $archive = [IO.Path]::GetFullPath((Join-Path $retained $name))
        $checkout = [IO.Path]::GetFullPath($checkout)
        # Resolve and contain both exact move targets immediately before the
        # operation. Retain the whole ordinary tree, never follow a junction or
        # replace an existing archive. Failure preserves diagnostics and refuses
        # a second clone rather than killing unrelated processes or reusing it.
        Assert-OrdinaryPathAncestors $checkout
        Assert-OrdinaryPathAncestors $archive
        if (-not(Test-Within $working $checkout) -or -not(Test-Within $working $archive) -or
            $checkout -ceq $working -or $archive -ceq $working -or
            (Test-Path -LiteralPath $archive) -or
            ([IO.File]::GetAttributes($checkout) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Independent checkout retention targets are not qualified.' }
        [IO.Directory]::Move($checkout, $archive)
        $builds.Add([pscustomobject]@{Name = $name; BuildPath = $checkout; RetainedCheckoutPath = $archive; SourceCommit = $head })
    }
    [pscustomobject]@{Builds = $builds.ToArray() }
}
Export-ModuleMember -Function Invoke-IndependentSourceBuildPair
