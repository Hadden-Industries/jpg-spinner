<# Exercise the real headless host's process-exit boundary, not just JUnit.
   The fixed scenarios never admit desktop/package worker tests. A completed
   report followed by a live process is a shutdown failure, not a test pass. #>
[CmdletBinding()]
param(
    [ValidateSet('DiagnosticSerialization', 'DiagnosticSuite', 'RecoveryDiagnostic')][string]$Scenario = 'RecoveryDiagnostic',
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Shutdown qualification output must be fresh and absent.' }
[void][IO.Directory]::CreateDirectory($output)
$report = Join-Path $output 'shutdown.junit.xml'
$specification = switch ($Scenario) {
    'DiagnosticSuite' { '[presentation][diagnostics]' }
    'RecoveryDiagnostic' { 'Recovery diagnostics observe the actual structured failure without changing recovery admission' }
    default { 'Diagnostics reject invalid UUIDs and durations before admitting a record' }
}
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = Join-Path $root 'artifacts/bin/x64/Release/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe'
$start.WorkingDirectory = Split-Path -Parent $start.FileName
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.Environment['JPG_SPINNER_TEST_EXECUTION_ENVIRONMENT'] = 'Headless'
foreach ($argument in @('--reporter', "JUnit::out=$report", '--rng-seed', '856262392', $specification,
        '~[ui-automation]', '~[assistive-technology]', '~[packaged-runtime]', '~[packaged-access-worker]')) {
    [void]$start.ArgumentList.Add($argument)
}
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
try {
    [void]$process.Start()
    $standardOutput = $process.StandardOutput.ReadToEndAsync()
    $standardError = $process.StandardError.ReadToEndAsync()
    # The selected case normally takes milliseconds; allow cold bootstrap and
    # host scheduling before declaring a teardown stall. This remains bounded
    # and requires actual exit, independently of a completed JUnit report.
    $exited = $process.WaitForExit((Get-QualificationTimeout -Category presentationShutdownProbe -Unit Milliseconds))
    if (-not $exited) {
        $process.Kill($true)
        if (-not $process.WaitForExit((Get-QualificationTimeout -Category nativeTeardown -Unit Milliseconds))) { throw 'Owned shutdown qualification host did not terminate.' }
    }
    $standardOutput.GetAwaiter().GetResult() | Write-Output
    $standardError.GetAwaiter().GetResult() | Write-Output
    if (-not $exited) { throw 'PRESENTATION_HOST_SHUTDOWN_TIMED_OUT: native host did not exit after its selected case.' }
    if ($process.ExitCode -ne 0) { throw "Native shutdown qualification case failed: $($process.ExitCode)." }
    [xml]$junit = Get-Content -LiteralPath $report -Raw
    if ([int]$junit.testsuites.testsuite.tests -lt 1 -or [int]$junit.testsuites.testsuite.failures -ne 0 -or
        [int]$junit.testsuites.testsuite.errors -ne 0 -or [int]$junit.testsuites.testsuite.skipped -ne 0) {
        throw 'Shutdown qualification did not execute a passing selected case.'
    }
}
finally { $process.Dispose() }
Write-Output "PASS: real $Scenario case and native process shutdown within the fixed budget."
