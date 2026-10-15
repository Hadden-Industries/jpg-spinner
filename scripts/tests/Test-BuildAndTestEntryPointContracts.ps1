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
    # These negative controls must not inherit an operator's live UI-lane
    # selection and accidentally deploy a package or acquire the foreground.
    [void]$startInfo.Environment.Remove('JPG_SPINNER_INTERACTIVE_TEST_LANE')
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
# entry point must fail before locating or launching a test executable when
# the caller has not selected an isolated interactive test lane.
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
    -ExpectedDiagnostic 'INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED'

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
    -ExpectedDiagnostic 'INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED'

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

# This static invocation contract complements the real governed build: MSBuild
# defaults to retaining parallel workers, which violates the script's bounded
# process lifetime. Parse PowerShell syntax rather than matching comments.
$buildTokens = $null
$buildParseErrors = $null
$buildSyntax = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $repositoryRoot 'scripts/Invoke-Build.ps1'),
    [ref]$buildTokens,
    [ref]$buildParseErrors
)
if ($buildParseErrors.Count -ne 0) { throw 'Build entry point has PowerShell syntax errors.' }
$argumentLoops = @($buildSyntax.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.ForEachStatementAst] -and
        $node.Variable.VariablePath.UserPath -ceq 'argument' -and
        @($node.Condition.FindAll({
            param($argumentNode)
            $argumentNode -is [System.Management.Automation.Language.VariableExpressionAst] -and
                $argumentNode.VariablePath.UserPath -ceq 'buildEntryPointPath'
        }, $true)).Count -eq 1
}, $true))
# Packaging legitimately appends another argument group. Identify the base
# native invocation by its project operand, not the number of foreach loops.
if ($argumentLoops.Count -ne 1) { throw 'Expected one MSBuild argument construction loop.' }
$nodeReuseArguments = @($argumentLoops[0].Condition.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.StringConstantExpressionAst] -and
        $node.Value -match '^[-/](nodeReuse|nr)(:|$)'
}, $true))
if ($nodeReuseArguments.Count -ne 1 -or $nodeReuseArguments[0].Value -cne '-nodeReuse:false') {
    throw 'Build entry point must explicitly disable MSBuild node reuse.'
}

# A bare -m expands to the host's processor count, not a memory-safe worker
# budget. Require an explicit bounded default and forward that selected count
# to MSBuild; native parameter validation owns rejecting nonpositive counts.
$parallelProcessArguments = @($argumentLoops[0].Condition.FindAll({
    param($node)
    ($node -is [System.Management.Automation.Language.StringConstantExpressionAst] -or
        $node -is [System.Management.Automation.Language.ExpandableStringExpressionAst]) -and
        $node.Value -match '^[-/](maxCpuCount|m)(:|$)'
}, $true))
if ($parallelProcessArguments.Count -ne 1 -or
    $parallelProcessArguments[0].Value -cne '-maxCpuCount:$MaximumBuildProcessCount') {
    throw 'Build entry point must pass an explicit MaximumBuildProcessCount to MSBuild.'
}
$parallelProcessParameters = @($buildSyntax.ParamBlock.Parameters | Where-Object {
    $_.Name.VariablePath.UserPath -ceq 'MaximumBuildProcessCount'
})
if ($parallelProcessParameters.Count -ne 1 -or
    $parallelProcessParameters[0].DefaultValue.SafeGetValue() -ne 2) {
    throw 'Build entry point must default to two concurrent MSBuild processes.'
}
Assert-RejectedInvocation `
    -RelativeScriptPath 'scripts/Invoke-Build.ps1' `
    -ArgumentList @('-Configuration', 'Debug', '-Architecture', 'x64', '-MaximumBuildProcessCount', '0') `
    -ExpectedDiagnostic 'minimum allowed range of 1'

# The native image check must be in the successful Release branch. Merely
# declaring /PDBALTPATH in XML cannot prove the final linker honored it.
$releaseImageGate = @($buildSyntax.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.IfStatementAst] -and
        $node.Clauses.Count -eq 1 -and
        $node.Clauses[0].Item1.Extent.Text -ceq '$buildExitCode -eq 0 -and $Configuration -ceq ''Release'''
}, $true))
if ($releaseImageGate.Count -ne 1) { throw 'Expected one successful-Release PE inspection branch.' }
$imageValidators = @($releaseImageGate[0].Clauses[0].Item2.FindAll({
    param($node)
    $node -is [System.Management.Automation.Language.StringConstantExpressionAst] -and
        $node.Value -ceq 'Test-PeProgramDatabaseReference.ps1'
}, $true))
if ($imageValidators.Count -ne 1) { throw 'Release builds must inspect the produced PE program-database reference.' }

# A UIA transaction timeout does not bound OS deployment or an entire test host.
# Require the runner's native Process deadline in its explicit Interactive branch.
# This syntax-level policy check complements, not replaces, isolated-lane runtime
# timeout/teardown qualification. PowerShell itself owns parsing; no source regex.
$testTokens = $null
$testParseErrors = $null
$testSyntax = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $repositoryRoot 'scripts/Invoke-TestSuite.ps1'),
    [ref]$testTokens,
    [ref]$testParseErrors
)
if ($testParseErrors.Count -ne 0) { throw 'Test entry point has PowerShell syntax errors.' }
$interactiveBoundedWaits = @($testSyntax.FindAll({
    param($node)
    if ($node -isnot [System.Management.Automation.Language.InvokeMemberExpressionAst] -or
        $node.Member.Value -cne 'WaitForExit' -or $null -eq $node.Arguments -or $node.Arguments.Count -ne 1 -or
        $node.Arguments[0] -isnot [System.Management.Automation.Language.ConstantExpressionAst] -or
        $node.Arguments[0].Value -ne 300000) { return $false }

    for ($ancestor = $node.Parent; $null -ne $ancestor; $ancestor = $ancestor.Parent) {
        if ($ancestor -isnot [System.Management.Automation.Language.IfStatementAst]) { continue }
        foreach ($clause in $ancestor.Clauses) {
            if ($clause.Item1.PipelineElements[0] -isnot
                [System.Management.Automation.Language.CommandExpressionAst]) { continue }
            $condition = $clause.Item1.PipelineElements[0].Expression
            if ($condition -is [System.Management.Automation.Language.BinaryExpressionAst] -and
                $condition.Operator -eq [System.Management.Automation.Language.TokenKind]::Ceq -and
                $condition.Left -is [System.Management.Automation.Language.VariableExpressionAst] -and
                $condition.Left.VariablePath.UserPath -ceq 'ExecutionEnvironment' -and
                $condition.Right -is [System.Management.Automation.Language.StringConstantExpressionAst] -and
                $condition.Right.Value -ceq 'Interactive' -and
                $node.Extent.StartOffset -ge $clause.Item2.Extent.StartOffset -and
                $node.Extent.EndOffset -le $clause.Item2.Extent.EndOffset) { return $true }
        }
    }
    return $false
}, $true))
if ($interactiveBoundedWaits.Count -ne 1) {
    throw 'Interactive test hosts must have one native 300-second Process.WaitForExit deadline.'
}

Write-Output (
    'PASS: build and test entry points reject unsupported configurations, architectures, ' +
    'project scopes, option-shaped test specifications, and execution environments without ' +
    'machine-wide vcpkg integration.'
)
