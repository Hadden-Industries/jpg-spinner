[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$toolchainResolverPath = Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1'
$legacyCppCxProjectPath = Join-Path $repositoryRoot 'JPG Spinner/JPG Spinner.vcxproj'
$modernBuildPolicyProbeProjectPath =
    Join-Path $repositoryRoot 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'

function Get-EvaluatedCppProjectBuildPolicy {
    param(
        [Parameter(Mandatory)]
        [string]$ProjectPath,

        [Parameter(Mandatory)]
        [string]$MSBuildExecutablePath
    )

    # MSBuild's evaluation switches expose the properties and item metadata
    # that a project actually consumes without executing a build. This keeps
    # the regression at the public build-system boundary instead of inferring
    # behavior from the source XML's import order.
    $evaluationOutput = & $MSBuildExecutablePath `
        $ProjectPath `
        -nologo `
        -noAutoResponse `
        '-p:Configuration=Debug' `
        '-p:Platform=x64' `
        '-getProperty:JpgSpinnerModernCppBuildPolicyEnabled' `
        '-getItem:ClCompile' `
        2>&1
    $evaluationExitCode = $LASTEXITCODE
    $evaluationText = $evaluationOutput -join "`n"
    if ($evaluationExitCode -ne 0) {
        throw (
            "MSBuild could not evaluate '$ProjectPath' (exit code $evaluationExitCode).`n" +
            $evaluationText
        )
    }

    try {
        return $evaluationText | ConvertFrom-Json -Depth 100
    }
    catch {
        throw "MSBuild returned invalid evaluation JSON for '$ProjectPath': $($_.Exception.Message)"
    }
}

if (-not (Test-Path -LiteralPath $toolchainResolverPath -PathType Leaf)) {
    throw "MSBuild toolchain resolver is absent: $toolchainResolverPath"
}
foreach ($requiredProjectPath in @(
    $legacyCppCxProjectPath,
    $modernBuildPolicyProbeProjectPath
)) {
    if (-not (Test-Path -LiteralPath $requiredProjectPath -PathType Leaf)) {
        throw "Required policy-scope project is absent: $requiredProjectPath"
    }
}

$resolvedToolchain = & $toolchainResolverPath -RepositoryRoot $repositoryRoot
$legacyProjectPolicy = Get-EvaluatedCppProjectBuildPolicy `
    -ProjectPath $legacyCppCxProjectPath `
    -MSBuildExecutablePath $resolvedToolchain.msBuildExecutablePath
$modernProbePolicy = Get-EvaluatedCppProjectBuildPolicy `
    -ProjectPath $modernBuildPolicyProbeProjectPath `
    -MSBuildExecutablePath $resolvedToolchain.msBuildExecutablePath

if (
    [string]$legacyProjectPolicy.Properties.JpgSpinnerModernCppBuildPolicyEnabled -cne
    'false'
) {
    throw 'The legacy C++/CX project must explicitly opt out of the modern C++ build policy.'
}

$legacyCompileItems = @($legacyProjectPolicy.Items.ClCompile)
$legacyCppCxCompileItems = @(
    $legacyCompileItems | Where-Object {
        [string]$_.CompileAsWinRT -ceq 'true'
    }
)
if ($legacyCppCxCompileItems.Count -eq 0) {
    throw 'The legacy regression fixture no longer contains an evaluated C++/CX compile item.'
}

$legacyCompileItemsUsingCpp20 = @(
    $legacyCompileItems | Where-Object {
        [string]$_.LanguageStandard -ceq 'stdcpp20'
    }
)
if ($legacyCompileItemsUsingCpp20.Count -ne 0) {
    $affectedCompileItemNames = @($legacyCompileItemsUsingCpp20 | ForEach-Object Identity)
    throw (
        'The modern C++20 build policy leaked into the legacy C++/CX project for: ' +
        ($affectedCompileItemNames -join ', ')
    )
}

if (
    [string]$modernProbePolicy.Properties.JpgSpinnerModernCppBuildPolicyEnabled -cne
    'true'
) {
    throw 'The modern build-policy probe must participate in the modern C++ build policy.'
}

$modernProbeCompileItems = @($modernProbePolicy.Items.ClCompile)
if ($modernProbeCompileItems.Count -eq 0) {
    throw 'The modern build-policy probe has no evaluated C++ compile items.'
}

$modernProbeItemsWithoutCxx20 = @(
    $modernProbeCompileItems | Where-Object {
        [string]$_.LanguageStandard -cne 'stdcpp20'
    }
)
if ($modernProbeItemsWithoutCxx20.Count -ne 0) {
    $unprotectedCompileItemNames = @($modernProbeItemsWithoutCxx20 | ForEach-Object Identity)
    throw (
        'The modern C++20 build policy did not reach the build-policy probe for: ' +
        ($unprotectedCompileItemNames -join ', ')
    )
}

Write-Output (
    'PASS: evaluated MSBuild metadata excludes the legacy C++/CX project from the modern ' +
    'C++ build policy while retaining C++20 for the modern build-policy probe.'
)
