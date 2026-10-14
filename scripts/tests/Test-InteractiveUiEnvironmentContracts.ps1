<#
.SYNOPSIS
Exercises preflight refusals without deploying packages or taking desktop focus.
.DESCRIPTION
Unaccepted declarations refuse before inspecting binaries. Accepted declarations
with a deliberately absent bundle refuse at the build prerequisite instead.
No case launches an app or touches the desktop.
This is environment qualification, not a product UI Automation assertion.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$powerShellPath = (Get-Process -Id $PID).Path

foreach ($case in @(
    @{ name = 'preflight'; script = 'scripts/Test-InteractiveUiEnvironment.ps1'; lane = $null; arguments = @() },
    @{ name = 'malformed lane'; script = 'scripts/Test-InteractiveUiEnvironment.ps1'; lane = 'isolatedvirtualmachine'; arguments = @() },
    @{ name = 'unaccepted lane'; script = 'scripts/Test-InteractiveUiEnvironment.ps1'; lane = 'LocalDesktop'; arguments = @() },
    # An explicit local-desktop declaration must not be mislabeled as isolation.
    # The nonexistent bundle keeps this positive syntax control side-effect free.
    @{ name = 'approved local desktop'; script = 'scripts/Test-InteractiveUiEnvironment.ps1';
        lane = 'OperatorApprovedLocalDesktop'; arguments = @('-RepositoryRoot',
            (Join-Path ([System.IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N'))));
        expectedReason = 'Build the matching presentation test host and packaged app first.' },
    @{ name = 'interactive runner'; script = 'scripts/Invoke-TestSuite.ps1'; lane = $null; arguments = @(
        '-Project', 'JpgSpinner.Presentation.Tests', '-ExecutionEnvironment', 'Interactive'
    ) }
)) {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powerShellPath
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    [void]$startInfo.Environment.Remove('JPG_SPINNER_INTERACTIVE_TEST_LANE')
    if ($null -ne $case.lane) {
        $startInfo.Environment['JPG_SPINNER_INTERACTIVE_TEST_LANE'] = $case.lane
    }
    foreach ($argument in @('-NoProfile', '-File', (Join-Path $repositoryRoot $case.script)) + $case.arguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $outputTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw "$($case.name) did not refuse before its bounded deadline."
        }
        $output = $outputTask.GetAwaiter().GetResult() + $errorTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 78 -or -not $output.Contains('INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED')) {
            throw "$($case.name) must refuse without an isolated lane (exit 78); observed $($process.ExitCode): $output"
        }
        if ($case.ContainsKey('expectedReason') -and -not $output.Contains($case.expectedReason)) {
            throw "$($case.name) must reach its build prerequisite; observed: $output"
        }
    }
    finally {
        $process.Dispose()
    }
}
Write-Output 'PASS: unqualified interactive execution is refused distinctly before package or desktop operations.'
