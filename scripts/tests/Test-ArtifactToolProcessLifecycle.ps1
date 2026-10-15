<# Test the actual producer's native child boundary through PowerShell's AST.
   A timeout must preserve diagnostics and prove the owned process exited;
   it must not lose those facts by throwing before output collection. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tokens = $null
$parseErrors = $null
$syntax = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $repositoryRoot 'scripts/New-ArtifactSbom.ps1'), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) { throw 'Artifact producer has syntax errors.' }
$helpers = @($syntax.FindAll({
    param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -ceq 'Invoke-ArtifactToolProcess'
}, $true))
if ($helpers.Count -ne 1) { throw 'Expected the actual native artifact process boundary.' }
$invokeProcess = $helpers[0].Body.GetScriptBlock()
$working = Join-Path $OwnedOutputRoot ('artifact-process-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($working)
$settings = @{}
$powershell = (Get-Process -Id $PID).Path
$child = Join-Path $PSScriptRoot 'Fixtures/ArtifactToolProcess.ps1'

foreach ($outcome in @('Complete','Fail','Timeout')) {
    $failure = $null
    try {
        & $invokeProcess -Executable $powershell -Arguments @('-NoProfile','-File',$child,'-Outcome',$outcome) `
            -LogName $outcome -TimeoutSeconds 5 | Out-Null
    }
    catch { $failure = $_.Exception.Message }
    $expected = switch ($outcome) { 'Complete' { $null } 'Fail' { 'exit 7' } 'Timeout' { 'exceeded its deadline' } }
    if (($null -eq $expected -and $null -ne $failure) -or
        ($null -ne $expected -and ($null -eq $failure -or -not $failure.Contains($expected, [StringComparison]::Ordinal)))) {
        throw "Wrong native process disposition for ${outcome}: $failure"
    }
    $stdoutPath = Join-Path $working "$outcome.stdout.log"
    $stderrPath = Join-Path $working "$outcome.stderr.log"
    if (-not [IO.File]::Exists($stdoutPath) -or -not [IO.File]::Exists($stderrPath)) {
        throw "Native $outcome child diagnostics were lost."
    }
    $stdout = [IO.File]::ReadAllText($stdoutPath).Trim()
    if ($stdout -notmatch '^owned-artifact-child:(\d+)$') { throw "Native $outcome stdout was not retained." }
    $childProcessId = [int]$Matches[1]
    if ([IO.File]::ReadAllText($stderrPath).Trim() -cne 'owned-artifact-stderr') {
        throw "Native $outcome stderr was not retained."
    }
    if ($null -ne (Get-Process -Id $childProcessId -ErrorAction SilentlyContinue)) {
        throw "Owned $outcome child has not become quiescent."
    }
}
Write-Output 'PASS: success, nonzero exit and deadline preserve native streams and leave no owned child running.'
