<# Real MSBuild SDK selection and a non-hosted missing-input refusal. No
   installer, registry writer, package restore, compilation or UI is invoked. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $root 'scripts/Install-HostedWindowsSdk.ps1'
$fixture = Join-Path $OwnedOutputRoot ('hosted-sdk-controls-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
& $installer -OutputDirectory (Join-Path $fixture 'installed')
$repository = Join-Path $fixture 'repository'
foreach ($relative in @('src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj', 'Directory.Build.props', 'eng/NativeLibraryDirectories.props', 'eng/toolchain-lock.json', '.vsconfig')) {
    $destination = Join-Path $repository $relative
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
}
# Alter only the owned project through the native XML DOM. Microsoft's ucrt.props
# honors this existing property, so its real macros select absent UCRT inputs.
# Installed SDK trees and the caller's environment remain untouched.
$projectPath = Join-Path $repository 'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj'
$document = [xml]::new()
$document.Load($projectPath)
$namespace = $document.DocumentElement.NamespaceURI
$propertyGroup = $document.CreateElement('PropertyGroup', $namespace)
$contentRoot = $document.CreateElement('UCRTContentRoot', $namespace)
$contentRoot.InnerText = (Join-Path $fixture 'absent-ucrt') + [IO.Path]::DirectorySeparatorChar
[void]$propertyGroup.AppendChild($contentRoot)
[void]$document.DocumentElement.PrependChild($propertyGroup)
$document.Save($projectPath)
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1') -Force
$rejected = $false
try {
    [void](Invoke-ArtifactToolProcess -Executable (Get-Process -Id $PID).Path `
        -Arguments @('-NoProfile', '-File', $installer, '-RepositoryRoot', $repository, '-OutputDirectory', (Join-Path $fixture 'missing')) `
        -WorkingDirectory $root -LogDirectory $fixture -LogName non-hosted-refusal -TimeoutSeconds 30 `
        -EnvironmentValues @{GITHUB_ACTIONS = 'false'; RUNNER_ENVIRONMENT = ''; RUNNER_OS = '' })
}
catch {
    $diagnostic = [IO.File]::ReadAllText((Join-Path $fixture 'non-hosted-refusal.stderr.log'))
    # PowerShell's native error renderer wraps long messages by console width.
    # Match the stable refusal prefix, not its presentation-dependent line wrap.
    if (-not $diagnostic.Contains('Missing pinned Windows SDK inputs;', [StringComparison]::Ordinal)) { throw }
    $rejected = $true
}
if (-not $rejected) { throw 'Missing local SDK inputs did not refuse before installer launch.' }
if ([IO.File]::Exists((Join-Path $fixture 'missing/installer-exit-code.txt'))) { throw 'The missing-input refusal reached the installer.' }
if (-not [IO.File]::Exists((Join-Path $fixture 'missing/missing-sdk-inputs.txt'))) { throw 'The refusal did not retain the actual missing SDK input inventory.' }
Write-Output 'PASS: real installed SDK selection and missing-UCRT non-hosted refusal; no installation or build performed.'
