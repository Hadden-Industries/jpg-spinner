[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$powerShellExecutablePath = (Get-Process -Id $PID).Path

function Invoke-EntryPoint {
    param(
        [Parameter(Mandatory)]
        [string]$RelativeScriptPath,

        [Parameter(Mandatory)]
        [string[]]$ArgumentList
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powerShellExecutablePath
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        '-NoProfile',
        '-File',
        (Join-Path $repositoryRoot $RelativeScriptPath)
    ) + $ArgumentList) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $standardOutputReadTask = $process.StandardOutput.ReadToEndAsync()
        $standardErrorReadTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()

        return [pscustomobject]@{
            exitCode = $process.ExitCode
            output = @(
                $standardErrorReadTask.GetAwaiter().GetResult().Trim()
                $standardOutputReadTask.GetAwaiter().GetResult().Trim()
            ) -join "`n"
        }
    }
    finally {
        $process.Dispose()
    }
}

function Assert-RejectedInvocation {
    param(
        [Parameter(Mandatory)]
        [string]$RelativeScriptPath,

        [Parameter(Mandatory)]
        [string[]]$ArgumentList,

        [Parameter(Mandatory)]
        [string]$ExpectedDiagnostic
    )

    $result = Invoke-EntryPoint `
        -RelativeScriptPath $RelativeScriptPath `
        -ArgumentList $ArgumentList
    if ($result.exitCode -eq 0) {
        throw "$RelativeScriptPath accepted forbidden arguments: $($ArgumentList -join ' ')"
    }
    if (
        $result.output.IndexOf(
            $ExpectedDiagnostic,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -lt 0
    ) {
        throw (
            "$RelativeScriptPath rejected the invocation without the required " +
            "'$ExpectedDiagnostic' diagnostic.`n$($result.output)"
        )
    }
}

Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-Build.ps1' `
    -ArgumentList @('-Configuration', 'Profile', '-Architecture', 'x64') `
    -ExpectedDiagnostic "parameter 'Configuration'"

Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-Build.ps1' `
    -ArgumentList @('-Configuration', 'Debug', '-Architecture', 'ARM') `
    -ExpectedDiagnostic "parameter 'Architecture'"

Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @('-Project', 'All', '-Architecture', 'x64') `
    -ExpectedDiagnostic "parameter 'Project'"

Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @(
        '-Project',
        'JpgSpinner.Domain.Tests',
        '-Architecture',
        'x64',
        '-ExecutionEnvironment',
        'Unattended'
    ) `
    -ExpectedDiagnostic "parameter 'ExecutionEnvironment'"

# Interactive execution is a materially different environment contract. The
# entry point must fail before locating or launching a test executable until
# the dedicated preflight script is implemented later in the plan.
Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @(
        '-Project',
        'JpgSpinner.Presentation.Tests',
        '-Architecture',
        'x64',
        '-ExecutionEnvironment',
        'Interactive'
    ) `
    -ExpectedDiagnostic 'Interactive test execution requires scripts/Test-InteractiveUiEnvironment.ps1'

# ValidateSet accepts casing variants unless IgnoreCase is disabled. The entry
# point deliberately accepts those PowerShell-native variants, so it must
# canonicalize them before applying environment and aggregate-suite policy.
Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @(
        '-Project',
        'jpgspinner.presentation.tests',
        '-Architecture',
        'X64',
        '-Configuration',
        'debug',
        '-ExecutionEnvironment',
        'interactive'
    ) `
    -ExpectedDiagnostic 'Interactive test execution requires scripts/Test-InteractiveUiEnvironment.ps1'

Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @(
        '-Project',
        'headlessall',
        '-Architecture',
        'x64',
        '-ExecutionEnvironment',
        'Interactive'
    ) `
    -ExpectedDiagnostic 'HeadlessAll cannot be combined with Interactive execution.'

# TestSpecification is a positional Catch2 filter, not an escape hatch for
# changing the runner-owned reporter, output, or execution mode. In particular,
# --list-tests exits successfully without executing tests and writes non-XML to
# the JUnit path when it is forwarded as an option.
Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-TestSuite.ps1' `
    -ArgumentList @(
        '-Project',
        'JpgSpinner.JpegTransformation.Tests',
        '-Architecture',
        'x64',
        '-TestSpecification',
        '--list-tests'
    ) `
    -ExpectedDiagnostic 'TestSpecification must be a Catch2 test specification, not a command-line option.'

foreach ($relativeScriptPath in @(
    'scripts/Invoke-Build.ps1',
    'scripts/Invoke-TestSuite.ps1'
)) {
    $scriptPath = Join-Path $repositoryRoot $relativeScriptPath
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        throw "Required entry point is absent: $relativeScriptPath"
    }

    $scriptText = [System.IO.File]::ReadAllText($scriptPath)
    if ($scriptText -match '(?i)\bvcpkg(?:\.exe)?\s+integrate\b') {
        throw "$relativeScriptPath must not mutate machine-wide vcpkg integration."
    }
}

Write-Output (
    'PASS: build and test entry points reject unsupported configurations, architectures, ' +
    'project scopes, option-shaped test specifications, and execution environments without ' +
    'machine-wide vcpkg integration.'
)
