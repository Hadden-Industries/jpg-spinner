<#
.SYNOPSIS
Checks native Catch2 selection admission without executing any test case.
.DESCRIPTION
Every child uses --list-tests. The real host and Catch2 parser own specification
semantics; this script neither splits commas nor imitates tag/name matching.
#>
[CmdletBinding()]
param([ValidateSet('x86', 'x64')][string]$Architecture = 'x64')
Set-StrictMode -Version Latest
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$platform = @{x86 = 'Win32'; x64 = 'x64' }[$Architecture]
$executable = Join-Path $root "artifacts/bin/$platform/Debug/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe"
function Assert-NativeSelection {
    param([string]$Specification, [int]$ExpectedExitCode)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $executable
    $start.WorkingDirectory = $root
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.Environment['JPG_SPINNER_TEST_EXECUTION_ENVIRONMENT'] = 'Headless'
    foreach ($argument in @($Specification, '~[ui-automation]', '~[assistive-technology]', '~[packaged-runtime]', '--list-tests')) {
        [void]$start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        [void]$process.Start()
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit((Get-QualificationTimeout -Category presentationSelectionProbe -Unit Milliseconds))) {
            $process.Kill($true)
            $process.WaitForExit()
            throw 'Native selection listing exceeded its 30-second bound; no test execution was requested.'
        }
        $text = $output.GetAwaiter().GetResult() + $errors.GetAwaiter().GetResult()
        if ($process.ExitCode -ne $ExpectedExitCode) {
            throw "Expected selection exit $ExpectedExitCode, got $($process.ExitCode) for '$Specification'.`n$text"
        }
        if ($ExpectedExitCode -eq 78 -and -not $text.Contains('PRESENTATION_TEST_SELECTION_NOT_ADMITTED', [StringComparison]::Ordinal)) {
            throw "Selection failed for the wrong reason: $text"
        }
        if ($ExpectedExitCode -eq 0 -and -not $text.Contains('matching test cases', [StringComparison]::Ordinal)) {
            throw "The positive control did not list native test cases: $text"
        }
    }
    finally { $process.Dispose() }
}
# Trailing exclusions constrain only the last comma-separated native filter in
# the locked Catch2 consumer. They must not admit UI or worker cases from an
# earlier alternative into a declared headless host.
Assert-NativeSelection '[presentation][automation],[presentation][view-model]' 78
Assert-NativeSelection '[packaged-access-worker],[presentation][view-model]' 78
Assert-NativeSelection '[presentation][view-model]' 0
Assert-NativeSelection '[presentation][metadata],[presentation][view-model]' 0
Write-Output 'PASS: native Catch2 admission rejects unsafe comma alternatives before execution and preserves safe selections.'
