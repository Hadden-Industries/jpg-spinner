[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet(
        'JpgSpinner.Domain.Tests',
        'JpgSpinner.JpegTransformation.Tests',
        'JpgSpinner.WindowsStorage.Tests',
        'JpgSpinner.BatchProcessing.Tests',
        'JpgSpinner.Presentation.Tests',
        'HeadlessAll'
    )]
    [string]$Project,

    [Parameter()]
    [AllowEmptyString()]
    [string]$TestSpecification = '',

    [Parameter()]
    [ValidateSet('x86', 'x64', 'ARM64')]
    [string]$Architecture = 'x64',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('Headless', 'Interactive')]
    [string]$ExecutionEnvironment = 'Headless',

    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $repositoryRoot"
}

# ValidateSet intentionally accepts casing variants by default. Convert each
# accepted spelling to the one repository-owned identity before exact policy
# comparisons, artifact lookup, or user-facing reporting.
function ConvertTo-CanonicalValidatedValue {
    param(
        [Parameter(Mandatory)]
        [string]$Value,

        [Parameter(Mandatory)]
        [string[]]$CanonicalValues
    )

    foreach ($canonicalValue in $CanonicalValues) {
        if ([string]::Equals(
                $Value,
                $canonicalValue,
                [System.StringComparison]::OrdinalIgnoreCase
            )) {
            return $canonicalValue
        }
    }

    # Parameter binding has already applied ValidateSet; reaching this branch
    # would mean the declaration and canonicalization table diverged.
    throw "No canonical repository identity is declared for accepted value '$Value'."
}

$Project = ConvertTo-CanonicalValidatedValue -Value $Project -CanonicalValues @(
    'JpgSpinner.Domain.Tests',
    'JpgSpinner.JpegTransformation.Tests',
    'JpgSpinner.WindowsStorage.Tests',
    'JpgSpinner.BatchProcessing.Tests',
    'JpgSpinner.Presentation.Tests',
    'HeadlessAll'
)
$Architecture = ConvertTo-CanonicalValidatedValue `
    -Value $Architecture `
    -CanonicalValues @('x86', 'x64', 'ARM64')
$Configuration = ConvertTo-CanonicalValidatedValue `
    -Value $Configuration `
    -CanonicalValues @('Debug', 'Release')
$ExecutionEnvironment = ConvertTo-CanonicalValidatedValue `
    -Value $ExecutionEnvironment `
    -CanonicalValues @('Headless', 'Interactive')

if (
    -not [string]::IsNullOrWhiteSpace($TestSpecification) -and
    $TestSpecification.TrimStart().StartsWith(
        '-',
        [System.StringComparison]::Ordinal
    )
) {
    # Catch2 reserves leading '-' arguments for its command-line options. This
    # parameter is deliberately limited to the positional <test-spec> grammar,
    # so callers cannot replace the repository-owned reporter or output path.
    throw 'TestSpecification must be a Catch2 test specification, not a command-line option.'
}

if ($Project -ceq 'HeadlessAll' -and $ExecutionEnvironment -cne 'Headless') {
    throw 'HeadlessAll cannot be combined with Interactive execution.'
}

if ($ExecutionEnvironment -ceq 'Interactive') {
    $interactiveEnvironmentTestPath =
        Join-Path $PSScriptRoot 'Test-InteractiveUiEnvironment.ps1'
    if (-not (Test-Path -LiteralPath $interactiveEnvironmentTestPath -PathType Leaf)) {
        throw (
            'Interactive test execution requires ' +
            'scripts/Test-InteractiveUiEnvironment.ps1, which is introduced ' +
            'with the UI Automation test environment later in the plan.'
        )
    }

    # Run the environment contract once before launching any test process; a
    # partial interactive run would otherwise produce misleading test results.
    & $interactiveEnvironmentTestPath -RepositoryRoot $repositoryRoot `
        -Architecture $Architecture -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$msBuildPlatformByArchitecture = @{
    x86 = 'Win32'
    x64 = 'x64'
    ARM64 = 'ARM64'
}
$msBuildPlatform = $msBuildPlatformByArchitecture[$Architecture]

$headlessTestProjects = @(
    'JpgSpinner.Domain.Tests',
    'JpgSpinner.JpegTransformation.Tests',
    'JpgSpinner.WindowsStorage.Tests',
    'JpgSpinner.BatchProcessing.Tests',
    'JpgSpinner.Presentation.Tests'
)
$selectedTestProjects =
    if ($Project -ceq 'HeadlessAll') { $headlessTestProjects } else { @($Project) }

$testExecutableByProject = [ordered]@{}
foreach ($selectedTestProject in $selectedTestProjects) {
    $testExecutablePath = Join-Path $repositoryRoot (
        "artifacts\bin\$msBuildPlatform\$Configuration\" +
        "$selectedTestProject\$selectedTestProject.exe"
    )
    if (-not (Test-Path -LiteralPath $testExecutablePath -PathType Leaf)) {
        throw (
            "Test executable is absent: $testExecutablePath. " +
            'Build the matching configuration and architecture first.'
        )
    }
    $testExecutableByProject[$selectedTestProject] = $testExecutablePath
}

$testResultsDirectory = Join-Path $repositoryRoot (
    "artifacts\TestResults\$Architecture\$Configuration"
)
[void][System.IO.Directory]::CreateDirectory($testResultsDirectory)

$firstFailingExitCode = 0
foreach ($selectedTestProject in $selectedTestProjects) {
    $testExecutablePath = $testExecutableByProject[$selectedTestProject]
    $jUnitReportPath = Join-Path $testResultsDirectory "$selectedTestProject.junit.xml"

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $testExecutablePath
    $startInfo.WorkingDirectory = Split-Path -Parent $testExecutablePath
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    # The native presentation host checks Catch2's parsed selection before any
    # test executes. Comma alternatives can escape trailing exclusion patterns;
    # this explicit process-scoped restriction makes such selections fail closed.
    $startInfo.Environment['JPG_SPINNER_TEST_EXECUTION_ENVIRONMENT'] = $ExecutionEnvironment
    foreach ($argument in @(
        '--reporter',
        "JUnit::out=$jUnitReportPath",
        '--name',
        $selectedTestProject
    )) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    if (-not [string]::IsNullOrWhiteSpace($TestSpecification)) {
        [void]$startInfo.ArgumentList.Add($TestSpecification)
    }
    if ($ExecutionEnvironment -ceq 'Headless') {
        # These native exclusions cover ordinary selections. The presentation
        # host additionally refuses unsafe selections after Catch2 parses them;
        # trailing exclusions alone do not constrain every comma alternative.
        [void]$startInfo.ArgumentList.Add('~[ui-automation]')
        [void]$startInfo.ArgumentList.Add('~[assistive-technology]')
        # AppContainer-only cases require an explicitly deployed native test
        # host. An unpackaged headless invocation cannot establish that boundary.
        [void]$startInfo.ArgumentList.Add('~[packaged-runtime]')
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $standardOutputReadTask = $process.StandardOutput.ReadToEndAsync()
        $standardErrorReadTask = $process.StandardError.ReadToEndAsync()
        $testTimedOut = $false
        if ($ExecutionEnvironment -ceq 'Interactive') {
            # Native UIA has per-provider timeouts, but deployment/activation and
            # the whole test case do not. Bound this owned host separately from
            # ordinary headless suites. Five minutes is the interactive host's
            # execution budget, not a retry/threshold extension after a failure.
            $testTimedOut = -not $process.WaitForExit(300000)
            if ($testTimedOut) {
                [Console]::Error.WriteLine(
                    'INTERACTIVE_TEST_PROCESS_TIMED_OUT: the test host exceeded 300 seconds. ' +
                    'Retain any printed UUID-owned package/process locator and reconcile it ' +
                    'before recycling its working copy; client termination does not prove ' +
                    'broker-activated app exit or package removal.'
                )
                # Only this child/process tree is targeted, not the deployment
                # service or a guessed app PID. Broker activation may outlive it.
                $process.Kill($true)
                if (-not $process.WaitForExit(5000)) {
                    throw 'The timed-out interactive test host did not exit after bounded termination.'
                }
            }
        }
        else {
            $process.WaitForExit()
        }

        $standardOutput = $standardOutputReadTask.GetAwaiter().GetResult()
        $standardError = $standardErrorReadTask.GetAwaiter().GetResult()
        if (-not [string]::IsNullOrWhiteSpace($standardOutput)) {
            [Console]::Out.Write($standardOutput)
        }
        if (-not [string]::IsNullOrWhiteSpace($standardError)) {
            [Console]::Error.Write($standardError)
        }

        # Run every selected executable to preserve a complete JUnit evidence
        # set, but return the first failure in the declared deterministic order.
        if ($testTimedOut) {
            # A partial/stale JUnit file is not a successful product result.
            # Do not continue interactive actions after uncertain teardown.
            $firstFailingExitCode = 124
            break
        }
        if ($process.ExitCode -ne 0 -and $firstFailingExitCode -eq 0) {
            $firstFailingExitCode = $process.ExitCode
        }
    }
    finally {
        $process.Dispose()
    }
}

exit $firstFailingExitCode
