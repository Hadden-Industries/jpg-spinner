[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$effectiveBuildPolicyPath = Join-Path $repositoryRoot 'scripts/Test-EffectiveBuildPolicy.ps1'
$domainProjectPath = Join-Path $repositoryRoot 'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj'
$originalDomainProjectBytes = [System.IO.File]::ReadAllBytes($domainProjectPath)
$originalDomainProjectHash = (Get-FileHash -LiteralPath $domainProjectPath -Algorithm SHA256).Hash
$powerShellExecutablePath = (Get-Process -Id $PID).Path

try {
    $domainProjectDocument = [System.Xml.XmlDocument]::new()
    $domainProjectDocument.PreserveWhitespace = $true
    $domainProjectDocument.Load($domainProjectPath)
    $msBuildNamespace = $domainProjectDocument.DocumentElement.NamespaceURI

    # Place the disabling token after every imported target. The probe remains
    # healthy, but the real Domain compilation consumes /sdl- as the rightmost
    # member of the option family. This proves the verifier observes each modern
    # project's actual import graph rather than extrapolating from the probe.
    $policyOverrideGroup = $domainProjectDocument.CreateElement(
        'ItemDefinitionGroup',
        $msBuildNamespace
    )
    $compilerDefinition = $domainProjectDocument.CreateElement(
        'ClCompile',
        $msBuildNamespace
    )
    $additionalOptions = $domainProjectDocument.CreateElement(
        'AdditionalOptions',
        $msBuildNamespace
    )
    $additionalOptions.InnerText = '%(AdditionalOptions) /sdl-'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$policyOverrideGroup.AppendChild($compilerDefinition)
    [void]$domainProjectDocument.DocumentElement.AppendChild($policyOverrideGroup)
    $domainProjectDocument.Save($domainProjectPath)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powerShellExecutablePath
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @(
        '-NoProfile',
        '-File',
        $effectiveBuildPolicyPath,
        '-EvidenceScope',
        'ModernSolution',
        '-ModernSolutionConfigurations',
        'Debug',
        '-ModernSolutionPlatforms',
        'x64'
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
        $combinedOutput = @(
            $standardErrorReadTask.GetAwaiter().GetResult().Trim()
            $standardOutputReadTask.GetAwaiter().GetResult().Trim()
        ) -join "`n"

        if ($process.ExitCode -eq 0) {
            throw 'Effective build policy accepted /sdl- in the real Domain project graph.'
        }
        foreach ($requiredDiagnostic in @('JpgSpinner.Domain', 'SDL-check')) {
            if ($combinedOutput.IndexOf(
                    $requiredDiagnostic,
                    [System.StringComparison]::OrdinalIgnoreCase
                ) -lt 0) {
                throw (
                    'Effective build policy rejected the project mutation without identifying ' +
                    "'$requiredDiagnostic'.`n$combinedOutput"
                )
            }
        }
    }
    finally {
        $process.Dispose()
    }
}
finally {
    # Restore exact bytes even when the verifier or an assertion fails. The
    # negative control must never leave a real project mutation in the worktree.
    [System.IO.File]::WriteAllBytes($domainProjectPath, $originalDomainProjectBytes)
}

$restoredDomainProjectHash = (Get-FileHash -LiteralPath $domainProjectPath -Algorithm SHA256).Hash
if ($restoredDomainProjectHash -cne $originalDomainProjectHash) {
    throw 'The modern-project policy negative control did not restore the Domain project bytes.'
}

Write-Output 'PASS: effective build policy rejects a rightmost disabling option in a real modern project graph.'
