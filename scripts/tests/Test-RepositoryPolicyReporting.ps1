[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$repositoryPolicyPath = Join-Path $repositoryRoot 'scripts/Test-RepositoryPolicy.ps1'
$centralPackageVersionsPath = Join-Path $repositoryRoot 'Directory.Packages.props'

$powerShellExecutablePath = (Get-Process -Id $PID).Path
$policyOutput = & $powerShellExecutablePath -NoProfile -File $repositoryPolicyPath 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "Repository policy must be green before its success report can be tested.`n$($policyOutput -join "`n")"
}

$successReport = $policyOutput -join "`n"
foreach ($requiredEvidenceLabel in @(
    'Visual Studio 2026/v145',
    'MSVC 14.51.36231',
    'Windows SDK 10.0.28000.0',
    'libjpeg-turbo 3.2.0',
    'Exiv2 0.28.8+xmp',
    'Catch2 3.16.0'
)) {
    if ($successReport.IndexOf($requiredEvidenceLabel, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Repository-policy success report omits verified evidence '$requiredEvidenceLabel'."
    }
}

# Task 1 has no NuGet package graph yet. A verifier must not present future,
# undeclared versions as evidence merely because the implementation plan names
# them; Task 2 will add and validate those pins before they enter this report.
if (-not (Test-Path -LiteralPath $centralPackageVersionsPath -PathType Leaf)) {
    foreach ($undeclaredNuGetVersion in @(
        'Windows App SDK 2.4.0',
        'C++/WinRT 3.0.260818.1',
        'SDK BuildTools 10.0.28000.2705'
    )) {
        if ($successReport.IndexOf($undeclaredNuGetVersion, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Repository policy reports undeclared NuGet evidence '$undeclaredNuGetVersion'."
        }
    }
}

Write-Output 'PASS: repository-policy success reporting is limited to configuration that the verifier actually parsed.'
