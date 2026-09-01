[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$nuGetResolutionVerifierPath = Join-Path $repositoryRoot 'scripts/Test-NuGetResolution.ps1'
$lockFilePath = Join-Path $repositoryRoot 'src/JpgSpinner.WindowsStorage/packages.lock.json'
$powerShellExecutablePath = (Get-Process -Id $PID).Path

if (-not (Test-Path -LiteralPath $lockFilePath -PathType Leaf)) {
    throw "Required NuGet lock-file fixture is absent: $lockFilePath"
}

function Invoke-NuGetResolutionVerifier {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powerShellExecutablePath
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        '-NoProfile',
        '-File',
        $nuGetResolutionVerifierPath,
        '-RepositoryRoot',
        $repositoryRoot
    )) {
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
            ExitCode = $process.ExitCode
            Output = @(
                $standardErrorReadTask.GetAwaiter().GetResult().Trim()
                $standardOutputReadTask.GetAwaiter().GetResult().Trim()
            ) -join "`n"
        }
    }
    finally {
        $process.Dispose()
    }
}

function Assert-NuGetResolutionRejected {
    param(
        [Parameter(Mandatory)]
        [string]$AcceptedFixtureMessage,

        [Parameter(Mandatory)]
        [string[]]$ExpectedDiagnostics
    )

    $verificationResult = Invoke-NuGetResolutionVerifier
    if ($verificationResult.ExitCode -eq 0) {
        throw $AcceptedFixtureMessage
    }

    # PowerShell's native error renderer can insert line wrapping into a child
    # process's diagnostic text. Collapse formatting whitespace on both sides
    # so the assertion tests semantic diagnostic content, not console width.
    $verificationOutputWithoutPowerShellGutters = [regex]::Replace(
        $verificationResult.Output,
        '\s+\|\s+',
        ' ',
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    $normalizedVerificationOutput = [regex]::Replace(
        $verificationOutputWithoutPowerShellGutters,
        '\s+',
        ' ',
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
    foreach ($expectedDiagnostic in $ExpectedDiagnostics) {
        $normalizedExpectedDiagnostic = [regex]::Replace(
            $expectedDiagnostic,
            '\s+',
            ' ',
            [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
        )
        if ($normalizedVerificationOutput.IndexOf(
                $normalizedExpectedDiagnostic,
                [System.StringComparison]::Ordinal
            ) -lt 0) {
            throw (
                'NuGet resolution verification rejected the fixture without ' +
                "reporting '$expectedDiagnostic'.`n$($verificationResult.Output)"
            )
        }
    }
}

$originalLockFileBytes = [System.IO.File]::ReadAllBytes($lockFilePath)
$originalLockFileHash = (Get-FileHash -LiteralPath $lockFilePath -Algorithm SHA256).Hash
$originalLockFileText = [System.Text.UTF8Encoding]::new($false, $true).GetString(
    $originalLockFileBytes
)

# Use equal duplicate values so a last-member-wins parser cannot accidentally
# turn this malformed authority into a different graph and expose the defect
# through an unrelated locked-restore mismatch.
$duplicateVersionLockFileText = $originalLockFileText.Replace(
    '  "version": 2,',
    '  "version": 2,' + "`r`n" + '  "version": 2,'
)
if ($duplicateVersionLockFileText -ceq $originalLockFileText) {
    throw 'The NuGet lock-file fixture lacks its expected version member.'
}

try {
    [System.IO.File]::WriteAllText(
        $lockFilePath,
        $duplicateVersionLockFileText,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-NuGetResolutionRejected `
        -AcceptedFixtureMessage 'NuGet resolution verification accepted a duplicate lock-file version member.' `
        -ExpectedDiagnostics @("Duplicate property 'version'")
}
finally {
    # Restore the exact original bytes even when the child verifier or this
    # assertion fails; policy tests must never leave dependency evidence dirty.
    [System.IO.File]::WriteAllBytes($lockFilePath, $originalLockFileBytes)
}

$restoredLockFileHash = (Get-FileHash -LiteralPath $lockFilePath -Algorithm SHA256).Hash
if ($restoredLockFileHash -cne $originalLockFileHash) {
    throw 'The NuGet negative control did not restore the original lock-file bytes.'
}

$centralPackageFilePath = Join-Path $repositoryRoot 'Directory.Packages.props'
$appProjectPath = Join-Path $repositoryRoot 'src/JpgSpinner.App/JpgSpinner.App.vcxproj'
$presentationTestProjectPath = Join-Path $repositoryRoot (
    'tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj'
)
$caseSemanticsFixturePaths = @(
    $centralPackageFilePath,
    $appProjectPath,
    $presentationTestProjectPath
)
foreach ($fixturePath in $caseSemanticsFixturePaths) {
    if (-not (Test-Path -LiteralPath $fixturePath -PathType Leaf)) {
        throw "Required NuGet policy fixture is absent: $fixturePath"
    }
}

$originalFixtureBytesByPath = @{}
$originalFixtureHashByPath = @{}
$originalFixtureTextByPath = @{}
$strictUtf8 = [System.Text.UTF8Encoding]::new($false, $true)
foreach ($fixturePath in $caseSemanticsFixturePaths) {
    $fixtureBytes = [System.IO.File]::ReadAllBytes($fixturePath)
    $originalFixtureBytesByPath[$fixturePath] = $fixtureBytes
    $originalFixtureHashByPath[$fixturePath] = (
        Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256
    ).Hash
    $originalFixtureTextByPath[$fixturePath] = $strictUtf8.GetString($fixtureBytes)
}

$centralPackageFileText = $originalFixtureTextByPath[$centralPackageFilePath]
$lineEnding = if ($centralPackageFileText.Contains("`r`n")) { "`r`n" } else { "`n" }
$mutatedCentralPackageFileText = $centralPackageFileText.Replace(
    '    <CentralPackageVersionOverrideEnabled>false</CentralPackageVersionOverrideEnabled>',
    '    <CentralPackageVersionOverrideEnabled>false</CentralPackageVersionOverrideEnabled>' +
        $lineEnding +
        '    <centralpackageversionoverrideenabled>true</centralpackageversionoverrideenabled>'
)
if ($mutatedCentralPackageFileText -ceq $centralPackageFileText) {
    throw 'Directory.Packages.props lacks the expected override-policy fixture.'
}

$appProjectText = $originalFixtureTextByPath[$appProjectPath]
$mutatedAppProjectText = $appProjectText.Replace(
    '    <PackageReference Include="Microsoft.WindowsAppSDK" />',
    '    <PackageReference Include="Microsoft.WindowsAppSDK">' +
        $lineEnding +
        '      <versionoverride>2.4.0</versionoverride>' +
        $lineEnding +
        '    </PackageReference>'
)
if ($mutatedAppProjectText -ceq $appProjectText) {
    throw 'The app project lacks the expected Windows App SDK package fixture.'
}

$presentationTestProjectText = $originalFixtureTextByPath[$presentationTestProjectPath]
$mutatedPresentationTestProjectText = $presentationTestProjectText.Replace(
    '  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />',
    '  <ItemGroup>' +
        $lineEnding +
        '    <PackageReference Include="Microsoft.Windows.CppWinRT" />' +
        $lineEnding +
        '  </ItemGroup>' +
        $lineEnding +
        '  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />'
)
if ($mutatedPresentationTestProjectText -ceq $presentationTestProjectText) {
    throw 'The presentation test project lacks the expected C++ targets import fixture.'
}

try {
    # MSBuild property, item, and metadata names are case-insensitive. These
    # deliberately noncanonical spellings prove that policy checks follow the
    # evaluated MSBuild identity rules instead of PowerShell's casing defaults.
    [System.IO.File]::WriteAllText(
        $centralPackageFilePath,
        $mutatedCentralPackageFileText,
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        $appProjectPath,
        $mutatedAppProjectText,
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        $presentationTestProjectPath,
        $mutatedPresentationTestProjectText,
        [System.Text.UTF8Encoding]::new($false)
    )

    Assert-NuGetResolutionRejected `
        -AcceptedFixtureMessage (
            'NuGet resolution verification accepted case-equivalent package policy ' +
            'bypasses in checked and previously unchecked projects.'
        ) `
        -ExpectedDiagnostics @(
            "Directory.Packages.props must declare CentralPackageVersionOverrideEnabled 'false' exactly once.",
            "src/JpgSpinner.App/JpgSpinner.App.vcxproj PackageReference 'Microsoft.WindowsAppSDK' must not declare VersionOverride",
            "tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj contains unapproved PackageReference 'Microsoft.Windows.CppWinRT'."
        )
}
finally {
    foreach ($fixturePath in $caseSemanticsFixturePaths) {
        [System.IO.File]::WriteAllBytes(
            $fixturePath,
            $originalFixtureBytesByPath[$fixturePath]
        )
    }
}

foreach ($fixturePath in $caseSemanticsFixturePaths) {
    $restoredFixtureHash = (
        Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256
    ).Hash
    if ($restoredFixtureHash -cne $originalFixtureHashByPath[$fixturePath]) {
        throw "The NuGet negative control did not restore fixture bytes: $fixturePath"
    }
}

Write-Output (
    'PASS: NuGet resolution rejects duplicate JSON members, case-equivalent ' +
    'MSBuild policy bypasses, and packages in every solution project.'
)
