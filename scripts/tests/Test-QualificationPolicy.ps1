<# Exercise the single policy authority and its actual native JSON/YAML/C++
   materialization boundary. Independent floors come from the accepted cold-
   runner contract and Microsoft's UIA/GitHub limits, not generated snapshots. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $root 'scripts/QualificationPolicy.psm1')
$sync = Join-Path $root 'scripts/Sync-QualificationPolicy.ps1'
& $sync -ToolsDirectory $ToolsDirectory
if ((Get-QualificationTimeout -Category nativeToolProcess) -lt 300) { throw 'Cold native qualification needs at least five minutes.' }
if ((Get-QualificationTimeout -Category uiAutomationTransaction) -lt 20) { throw 'UIA qualification cannot impose a transaction ceiling below the native default.' }
if ((Get-QualificationTimeout -Category hostedJobLimit -Unit Minutes) -gt 360) { throw 'Qualification cannot exceed the GitHub-hosted execution limit.' }
$message = $null
try { [void](Get-QualificationTimeout -Category uiStateTransition -Unit Minutes) } catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('must use whole minutes', [StringComparison]::Ordinal)) { throw 'Native workflow unit conversion rounded a fractional minute.' }
$teardown = Get-QualificationTimeout -Category nativeTeardown
if ((Get-QualificationTimeout -Category nestedPrerequisiteProbe) -le (Get-QualificationTimeout -Category nativeToolProcess) + 2 * $teardown) {
    throw 'A prerequisite-control parent must outlive its child execution and teardown.'
}
if ((Get-QualificationTimeout -Category hostedPrerequisiteStep) -le
    (Get-QualificationTimeout -Category nativeToolProcess) + (Get-QualificationTimeout -Category artifactDownload) +
    (Get-QualificationTimeout -Category runtimeInstallation) + 2 * $teardown) {
    throw 'Hosted runtime setup needs an envelope for selection, acquisition, installation and teardown.'
}
$fixture = Join-Path $OwnedOutputRoot ('qualification-policy-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$original = [IO.File]::ReadAllText((Join-Path $root 'eng/qualification-policy.json'))
foreach ($case in @(
    @{name = 'duplicate-member'; text = $original.Replace('"schemaVersion": 1,', '"schemaVersion": 1, "schemaVersion": 1,'); diagnostic = 'Duplicate' },
    @{name = 'array-root'; text = '[' + $original + ']'; diagnostic = 'object' },
    @{name = 'fractional-duration'; text = $original.Replace('"nativeToolProcess": 600', '"nativeToolProcess": 600.5'); diagnostic = 'schema' },
    @{name = 'string-duration'; text = $original.Replace('"nativeToolProcess": 600', '"nativeToolProcess": "600"'); diagnostic = 'schema' },
    @{name = 'zero-duration'; text = $original.Replace('"nativeToolProcess": 600', '"nativeToolProcess": 0'); diagnostic = 'schema' }
)) {
    if ($case.text -ceq $original) { throw "Independent policy fixture precondition differs: $($case.name)" }
    $path = Join-Path $fixture ($case.name + '.json')
    [IO.File]::WriteAllText($path, $case.text)
    $message = $null
    try { [void](Read-QualificationPolicy -Path $path) } catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.diagnostic, [StringComparison]::OrdinalIgnoreCase)) { throw "Policy admitted $($case.name) or failed at another boundary: $message" }
}
# A caller-supplied native JsonElement can still contain duplicate members.
# Prove that this public path also revalidates raw JSON before value selection.
$duplicateDocument = [Text.Json.JsonDocument]::Parse($original.Replace('"schemaVersion": 1,', '"schemaVersion": 1, "schemaVersion": 1,'))
try {
    $message = $null
    try { [void](Get-QualificationTimeout -Policy $duplicateDocument.RootElement -Category nativeToolProcess) } catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains('Duplicate', [StringComparison]::OrdinalIgnoreCase)) { throw 'Explicit native policy values bypassed duplicate-member rejection.' }
} finally { $duplicateDocument.Dispose() }

# Keep a complete owned source fixture, then change only one generated scalar.
# This tests actual stale-output refusal, not another copy of the generator.
$repository = Join-Path $fixture 'repository'
$policy = Get-QualificationPolicy
$paths = @('eng/qualification-policy.json', 'tests/JpgSpinner.Presentation.Tests/QualificationTiming.h') +
    @($policy.GetProperty('workflowJobCategories').EnumerateObject() | ForEach-Object { '.github/workflows/' + $_.Name })
foreach ($relative in $paths) {
    $destination = Join-Path $repository $relative
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
}
& $sync -ToolsDirectory $ToolsDirectory -RepositoryRoot $repository
$workflow = Join-Path $repository '.github/workflows/continuous-integration.yml'
$source = [IO.File]::ReadAllText($workflow)
if (-not $source.Contains('timeout-minutes: 180', [StringComparison]::Ordinal)) { throw 'Stale-workflow fixture lacks the independent native-build budget.' }
[IO.File]::WriteAllText($workflow, $source.Replace('timeout-minutes: 180', 'timeout-minutes: 179'))
$message = $null
try { & $sync -ToolsDirectory $ToolsDirectory -RepositoryRoot $repository } catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Generated qualification policy values are stale:', [StringComparison]::Ordinal)) { throw 'Stale workflow policy values were admitted.' }
[IO.File]::WriteAllText($workflow, $source)
$header = Join-Path $repository 'tests/JpgSpinner.Presentation.Tests/QualificationTiming.h'
[IO.File]::AppendAllText($header, '// Deliberately stale derived file.')
$message = $null
try { & $sync -ToolsDirectory $ToolsDirectory -RepositoryRoot $repository } catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Generated qualification timing header is stale.', [StringComparison]::Ordinal)) { throw 'Stale C++ policy values were admitted.' }
Write-Output 'PASS: canonical policy, nested budgets, independent platform bounds, strict JSON types/uniqueness and native stale-materialization refusals.'
