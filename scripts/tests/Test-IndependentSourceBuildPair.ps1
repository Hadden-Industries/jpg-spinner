<# Real Git/filesystem lifecycle controls, without another application rebuild.
   The build action is the expensive external operation; only it is replaced by
   an owned marker writer/refusal. The checkout/path/retention decisions stay real. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $root scripts/ArtifactToolProcess.psm1) -Force
Import-Module (Join-Path $root scripts/IndependentSourceBuildPair.psm1) -Force
$nativeInvoker = Get-Command Invoke-ArtifactToolProcess -Module ArtifactToolProcess -ErrorAction Stop
$git = (Get-Command git -CommandType Application | Select-Object -First 1).Source
$commit = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot select the native source fixture.' }
$fixture = Join-Path $OwnedOutputRoot ('independent-source-pair-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$visits = [Collections.Generic.List[object]]::new()
$action = {
    param($Checkout, $Name)
    $marker = Join-Path $Checkout artifacts/independent-build-marker.txt
    if (Test-Path -LiteralPath $marker) { throw 'An independent build inherited prior intermediates.' }
    if (Test-Path -LiteralPath (Join-Path $Checkout .git/objects/info/alternates)) { throw 'An independent clone shares the source object store.' }
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $marker))
    # Exercise the production driver's actual closure/invoker binding pattern
    # with a tiny native Git operation instead of a compiler invocation.
    $observed = & $nativeInvoker -Executable $git -WorkingDirectory $Checkout -LogDirectory (Split-Path -Parent $marker) `
        -LogName observed-source -TimeoutSeconds (Get-QualificationTimeout -Category gitObservation) -Arguments @('rev-parse', 'HEAD')
    if ($observed.Trim() -cne $commit) { throw 'Build callback did not receive the exact selected source.' }
    [IO.File]::WriteAllText($marker, $Name)
    $visits.Add([pscustomobject]@{Path = $Checkout; Name = $Name })
}.GetNewClosure()
$result = Invoke-IndependentSourceBuildPair -SourceRepositoryRoot $root -SourceCommit $commit `
    -WorkingDirectory (Join-Path $fixture work) -LogDirectory (Join-Path $fixture logs) -BuildAction $action
if ($visits.Count -ne 2 -or $visits[0].Name -cne 'first' -or $visits[1].Name -cne 'second') { throw 'The independent build pair did not execute both selected passes.' }
if ($visits[0].Path -cne $visits[1].Path) { throw 'Independent builds did not hold the specified absolute source path constant.' }
foreach ($build in $result.Builds) {
    if ($build.BuildPath -cne $visits[0].Path -or -not [IO.Directory]::Exists($build.RetainedCheckoutPath)) { throw 'Retained build identity is inaccurate.' }
    $marker = Join-Path $build.RetainedCheckoutPath artifacts/independent-build-marker.txt
    if ([IO.File]::ReadAllText($marker) -cne $build.Name) { throw 'Retention lost or mixed independent build outputs.' }
    $retainedHead = & git -C $build.RetainedCheckoutPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $retainedHead -cne $commit) { throw 'Retained native checkout no longer identifies the selected source.' }
}
if (Test-Path -LiteralPath $visits[0].Path) { throw 'The completed source slot was not released after retention.' }
$refused = $null
try { Invoke-IndependentSourceBuildPair -SourceRepositoryRoot $root -SourceCommit $commit -WorkingDirectory (Join-Path $fixture work) -LogDirectory (Join-Path $fixture unused-logs) -BuildAction { throw 'Must not run' } | Out-Null }
catch { $refused = $_.Exception.Message }
if ($null -eq $refused -or (Test-Path -LiteralPath (Join-Path $fixture unused-logs))) { throw 'Existing work did not refuse before effects.' }
$refused = $null
$failureVisits = [Collections.Generic.List[string]]::new()
$failureAction = { param($Checkout, $Name) $failureVisits.Add($Name); throw 'Controlled build refusal.' }.GetNewClosure()
try { Invoke-IndependentSourceBuildPair -SourceRepositoryRoot $root -SourceCommit $commit -WorkingDirectory (Join-Path $fixture refused-work) -LogDirectory (Join-Path $fixture refused-logs) -BuildAction $failureAction | Out-Null }
catch { $refused = $_.Exception.Message }
if ($refused -cne 'Controlled build refusal.' -or $failureVisits.Count -ne 1 -or $failureVisits[0] -cne 'first' -or
    (Test-Path -LiteralPath (Join-Path $fixture refused-logs/second))) { throw 'An unsuccessful build was hidden or allowed a second pass.' }
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $root README.md)).Hash
$changedSourceAction = { param($Checkout, $Name) [IO.File]::WriteAllText((Join-Path $Checkout README.md), 'Intentional change in this owned clone only.') }
$refused = $null
try { Invoke-IndependentSourceBuildPair -SourceRepositoryRoot $root -SourceCommit $commit -WorkingDirectory (Join-Path $fixture changed-source-work) -LogDirectory (Join-Path $fixture changed-source-logs) -BuildAction $changedSourceAction | Out-Null }
catch { $refused = $_.Exception.Message }
if ($refused -cne 'Build action changed selected source inputs.' -or
    (Test-Path -LiteralPath (Join-Path $fixture changed-source-logs/second)) -or
    (Get-FileHash -LiteralPath (Join-Path $root README.md)).Hash -cne $sourceHash) { throw 'Changed source was accepted, retried or leaked into the original repository.' }
Write-Output 'PASS: real independent clones use one specified build path, inherit no intermediates, retain both complete trees and refuse unsafe/reused work.'
