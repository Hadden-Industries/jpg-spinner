<# Qualify actual Microsoft installer identities without executing them.
   Real native project evaluation plus owned refusal inputs exercise the public
   setup boundary; the hosted installation branch needs deliberate hosted QA. #>
[CmdletBinding()]
param(
    [string]$X64InstallerPath,
    [string]$X86InstallerPath,
    [string]$Arm64InstallerPath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $OwnedOutputRoot ('hosted-runtime-controls-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$setup = Join-Path $root 'scripts/Install-HostedWindowsAppRuntime.ps1'
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1') -Force
Import-Module (Join-Path $root 'scripts/JsonObjectMemberValidation.psm1') -Force
$context = @{Executable = (Get-Process -Id $PID).Path; WorkingDirectory = $root; LogDirectory = $fixture;
    EnvironmentValues = @{GITHUB_ACTIONS = 'false'; RUNNER_ENVIRONMENT = ''; RUNNER_OS = '' }; TimeoutSeconds = (Get-QualificationTimeout -Category nestedPrerequisiteProbe)
}
$installerCases = @(@(
        @{architecture = 'x64'; path = $X64InstallerPath; sha256 = '931a421e8dc3e6e67724806cb67fecdbb88dfe323f0170842eb4a4b4b149f1e2' },
        @{architecture = 'x86'; path = $X86InstallerPath; sha256 = '76dbd7c272cee0bf18f0b7228255b353d669cd55dc530209c45646f47acb89d4' }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_.path) })
if (-not [string]::IsNullOrWhiteSpace($Arm64InstallerPath)) {
    $installerCases += @{architecture = 'ARM64'; path = $Arm64InstallerPath; sha256 = 'd5e4d34547eb4e31c64bf1532415b3019c0d92b750e72eb18d3d95bd00feacbb' }
}
if ($installerCases.Count -eq 0) { throw 'Supply at least one actual Microsoft runtime installer.' }
$referenceInstallerPath = $installerCases[0].path
foreach ($case in $installerCases) {
    $output = Join-Path $fixture $case.architecture
    [void](Invoke-ArtifactToolProcess @context -LogName ($case.architecture + '-whatif') -Arguments @(
            '-NoProfile', '-File', $setup, '-Architecture', $case.architecture, '-InstallerPath', $case.path, '-OutputDirectory', $output, '-WhatIf'
        ))
    $identity = ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText((Join-Path $output 'runtime-installer-identity.json'))) `
        -RequiredRootValueKind Object -SourceDescription 'Qualified Microsoft runtime installer' -AsHashtable
    if ($identity.windowsAppSdkPackageVersion -cne '2.5.1' -or $identity.architecture -cne $case.architecture -or
        $identity.sha256 -cne $case.sha256 -or $identity.fileVersion -cne '2.5' -or $identity.signatureStatus -cne 'Valid') {
        throw 'Actual Microsoft runtime installer does not match its independently measured release identity.'
    }
    if ([IO.File]::Exists((Join-Path $output 'installer-exit-code.txt'))) { throw 'WhatIf reached runtime installation.' }
}
# Native MSBuild also evaluates this deliberately minimal, owned project. The
# fixture changes the package-selection input, never installed packages or the
# real checkout. Both a different SDK and ambiguous duplicate selection refuse.
$repository = Join-Path $fixture 'selection-repository'
$project = Join-Path $repository 'Directory.Packages.props'
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($project))
[void][IO.Directory]::CreateDirectory((Join-Path $repository 'eng'))
Copy-Item -LiteralPath (Join-Path $root 'eng/toolchain-lock.json') -Destination (Join-Path $repository 'eng/toolchain-lock.json')
foreach ($case in @(
        @{name = 'different-sdk'; versions = @('2.5.0') },
        @{name = 'duplicate-sdk'; versions = @('2.5.1', '2.5.1') }
    )) {
    $document = [xml]'<Project><ItemGroup /></Project>'
    foreach ($version in $case.versions) {
        $item = $document.CreateElement('PackageVersion')
        $item.SetAttribute('Include', 'Microsoft.WindowsAppSDK')
        $item.SetAttribute('Version', $version)
        [void]$document.DocumentElement.FirstChild.AppendChild($item)
    }
    $document.Save($project)
    $output = Join-Path $fixture $case.name
    $rejected = $false
    try {
        [void](Invoke-ArtifactToolProcess @context -LogName $case.name -Arguments @(
                '-NoProfile', '-File', $setup, '-Architecture', 'x64', '-InstallerPath', $referenceInstallerPath,
                '-OutputDirectory', $output, '-RepositoryRoot', $repository, '-WhatIf'
            ))
    }
    catch {
        $diagnostic = [IO.File]::ReadAllText((Join-Path $fixture ($case.name + '.stderr.log')))
        if (-not $diagnostic.Contains('pins do not match the unique adopted', [StringComparison]::Ordinal)) { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Runtime setup accepted $($case.name)." }
    if ([IO.File]::Exists((Join-Path $output 'runtime-installer-identity.json'))) { throw 'An incompatible SDK reached binary qualification.' }
}
foreach ($case in @(
        @{name = 'non-hosted-installation'; arguments = @('-InstallerPath', $referenceInstallerPath); diagnostic = 'installation is restricted to an ephemeral' },
        @{name = 'whatif-without-binary'; arguments = @('-WhatIf'); diagnostic = 'qualification requires an existing installer' },
        @{name = 'wrong-binary'; arguments = @('-WhatIf', '-InstallerPath', (Join-Path $root '.vsconfig')); diagnostic = 'installer SHA-256 differs' }
    )) {
    $output = Join-Path $fixture $case.name
    $rejected = $false
    try {
        [void](Invoke-ArtifactToolProcess @context -LogName $case.name -Arguments (@(
                    '-NoProfile', '-File', $setup, '-Architecture', 'x64', '-OutputDirectory', $output
                ) + $case.arguments))
    }
    catch {
        $diagnostic = [IO.File]::ReadAllText((Join-Path $fixture ($case.name + '.stderr.log')))
        if (-not $diagnostic.Contains($case.diagnostic, [StringComparison]::Ordinal)) { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Runtime prerequisite accepted $($case.name)." }
    if ([IO.File]::Exists((Join-Path $output 'installer-exit-code.txt'))) { throw 'A refused input reached runtime installation.' }
}
Write-Output 'PASS: native release identities, unique central SDK association, WhatIf isolation, local acquisition/installation refusals and wrong-binary refusal; no runtime was installed.'
