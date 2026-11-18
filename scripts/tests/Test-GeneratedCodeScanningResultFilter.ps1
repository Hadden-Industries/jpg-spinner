<# The CodeQL upload filter removes only results in C++/WinRT projection headers.
   XAML-generated sources, first-party code and location-less results remain. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OwnedOutputRoot) (
    'codeql-result-filter-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$sarif = @'
{"version":"2.1.0","runs":[{"tool":{"driver":{"name":"CodeQL"}},
"artifacts":[{"location":{"uri":"artifacts/obj/x64/Release/JpgSpinner.Presentation.Tests/Generated%20Files/winrt/base.h"}}],
"results":[
{"ruleId":"projection-by-uri","locations":[{"physicalLocation":{"artifactLocation":{"uri":"src/JpgSpinner.App/Generated Files/winrt/base.h"}}}]},
{"ruleId":"projection-by-index","locations":[{"physicalLocation":{"artifactLocation":{"index":0}}}]},
{"ruleId":"xaml-generated","locations":[{"physicalLocation":{"artifactLocation":{"uri":"src/JpgSpinner.App/Generated%20Files/MainWindow.g.h"}}}]},
{"ruleId":"first-party","locations":[{"physicalLocation":{"artifactLocation":{"uri":"src/JpgSpinner.Domain/Rotation.cpp"}}}]},
{"ruleId":"lookalike","locations":[{"physicalLocation":{"artifactLocation":{"uri":"src/NotGenerated Files/winrt/base.h"}}}]},
{"ruleId":"no-location"}
]}]}
'@
[IO.File]::WriteAllText((Join-Path $fixture 'cpp.sarif'), $sarif)
try {
    $output = & (Join-Path $repositoryRoot 'scripts/Remove-GeneratedCodeScanningResults.ps1') -SarifDirectory $fixture
    if ($output -cne 'Removed 2 result(s) located in generated C++/WinRT projection headers.') {
        throw "Unexpected filter report: $output"
    }
    $remaining = @((Get-Content -LiteralPath (Join-Path $fixture 'cpp.sarif') -Raw | ConvertFrom-Json).runs[0].results.ruleId)
    if (($remaining -join ',') -cne 'xaml-generated,first-party,lookalike,no-location') {
        throw "Filter removed or retained the wrong results: $($remaining -join ', ')"
    }
    $empty = Join-Path $fixture 'empty'
    [void][IO.Directory]::CreateDirectory($empty)
    $refused = $false
    try { & (Join-Path $repositoryRoot 'scripts/Remove-GeneratedCodeScanningResults.ps1') -SarifDirectory $empty | Out-Null }
    catch { $refused = $_.Exception.Message.Contains('No SARIF output exists') }
    if (-not $refused) { throw 'Filter must refuse a directory without SARIF output.' }
}
finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Output 'PASS: CodeQL upload filter removes only C++/WinRT projection-header results.'
