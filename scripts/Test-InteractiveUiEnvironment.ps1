<#
.SYNOPSIS
Qualifies the desktop and isolated package deployment used by UI acceptance tests.
.DESCRIPTION
Run only in an accepted isolated VM, dedicated self-hosted interactive lane,
or an explicitly operator-approved local desktop session.
JPG_SPINNER_INTERACTIVE_TEST_LANE selects that operator-owned context; it is not
proof of isolation. Native checks still prove active/unlocked desktop, foreground
focus, UI Automation availability, and actual test-package registration. If the
background host cannot activate its owned probe, ordinary operator/Computer Use
activation has a bounded 30-second window; actual foreground focus remains required.
Exit 78 denotes unavailable qualification, never a product failure or a skip.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Stop-UnqualifiedInteractiveEnvironment {
    param([Parameter(Mandatory)][string]$Reason)
    [Console]::Error.WriteLine("INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED: $Reason")
    exit 78
}

# Do not infer dedicated test-host ownership from UserInteractive, developer mode,
# a session number, or CI=true. This selection is an operator declaration; the
# selected context and its limitations remain a separate acceptance obligation.
$interactiveLane = [Environment]::GetEnvironmentVariable('JPG_SPINNER_INTERACTIVE_TEST_LANE', 'Process')
if ($interactiveLane -cnotin @('IsolatedVirtualMachine', 'DedicatedSelfHosted', 'OperatorApprovedLocalDesktop')) {
    Stop-UnqualifiedInteractiveEnvironment -Reason (
        'Select the approved interactive lane with ' +
        'JPG_SPINNER_INTERACTIVE_TEST_LANE=IsolatedVirtualMachine, DedicatedSelfHosted, or OperatorApprovedLocalDesktop. ' +
        'This command does not configure a host or grant desktop access.'
    )
}

try {
    $resolvedRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
    $platform = @{ x86 = 'Win32'; x64 = 'x64'; ARM64 = 'ARM64' }[$Architecture]
    $binaryRoot = Join-Path $resolvedRoot "artifacts/bin/$platform/$Configuration"
    $testExecutable = Join-Path $binaryRoot 'JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe'
    $applicationDirectory = Join-Path $binaryRoot 'JpgSpinner.App'
    if (-not (Test-Path -LiteralPath $testExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $applicationDirectory 'AppxManifest.xml') -PathType Leaf)) {
        Stop-UnqualifiedInteractiveEnvironment -Reason 'Build the matching presentation test host and packaged app first.'
    }

    # Use the SDK-based native test host, not an Add-Type reproduction of Windows
    # COM interfaces, WTS structs or the WinRT package-deployment implementation.
    # Its explicit mode performs qualification only; no product test runs here.
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $testExecutable
    $startInfo.WorkingDirectory = $resolvedRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    [void]$startInfo.ArgumentList.Add('--qualify-interactive-ui')
    [void]$startInfo.ArgumentList.Add($applicationDirectory)
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $outputTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit(120000)
        if ($timedOut) {
            # Bound only our child, never the OS deployment service. An in-flight
            # registration can outlive its client: keep its UUID-owned working
            # files (printed before registration) for explicit reconciliation.
            # Process termination is not evidence of package removal.
            $process.Kill($true)
            $process.WaitForExit()
        }
        [Console]::Out.Write($outputTask.GetAwaiter().GetResult())
        [Console]::Error.Write($errorTask.GetAwaiter().GetResult())
        if ($timedOut) {
            Stop-UnqualifiedInteractiveEnvironment -Reason (
                'Native qualification exceeded 120 seconds. Any printed owned ' +
                'package fixture is retained; reconcile registration before recycling it.'
            )
        }
        if ($process.ExitCode -ne 0) {
            Stop-UnqualifiedInteractiveEnvironment -Reason "Native qualification returned $($process.ExitCode)."
        }
    }
    finally {
        $process.Dispose()
    }
    exit 0
}
catch {
    Stop-UnqualifiedInteractiveEnvironment -Reason $_.Exception.Message
}
