<# Satisfy the project's existing Windows SDK selection on an ephemeral hosted
   runner. MSBuild owns SDK/property selection; Visual Studio Installer owns
   component acquisition. An already complete local installation is inspected
   read-only. Missing local components never trigger a workstation installation.
   https://learn.microsoft.com/visualstudio/install/use-command-line-parameters-to-install-visual-studio
   https://learn.microsoft.com/powershell/module/microsoft.powershell.management/start-process #>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Windows SDK prerequisite diagnostics require fresh output.' }
[void][IO.Directory]::CreateDirectory($output)
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
$context = @{Executable = $toolchain.msBuildExecutablePath; WorkingDirectory = $root; LogDirectory = $output }

function Get-NativeWindowsSdkPrerequisites {
    param([string]$LogName)
    # -getProperty evaluates the real project without invoking build targets,
    # restore, compilation or the failing native environment-publication step.
    $json = Invoke-ArtifactToolProcess @context -LogName $LogName -Arguments @(
        (Join-Path $root 'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj'),
        '-nologo', '-noAutoResponse', '-nodeReuse:false', '-maxCpuCount:1',
        '-p:Configuration=Release', '-p:Platform=x64',
        '-getProperty:WindowsTargetPlatformVersion,WindowsSdkDir,UCRTContentRoot,TargetUniversalCRTVersion'
    )
    $properties = (ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -RequiredRootValueKind Object `
        -SourceDescription 'Native MSBuild Windows SDK selection' -AsHashtable).Properties
    foreach ($name in @('WindowsTargetPlatformVersion', 'WindowsSdkDir', 'UCRTContentRoot', 'TargetUniversalCRTVersion')) {
        if (-not $properties.Contains($name) -or [string]::IsNullOrWhiteSpace($properties[$name])) {
            throw "Native MSBuild omitted its SDK selection property: $name"
        }
    }
    $sdkVersion = [version]$properties.WindowsTargetPlatformVersion
    $component = "Microsoft.VisualStudio.Component.Windows11SDK.$($sdkVersion.Build)"
    $configuration = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $root '.vsconfig') -Raw) `
        -RequiredRootValueKind Object -SourceDescription 'Required Visual Studio components' -AsHashtable
    if ($component -cnotin $configuration.components) { throw 'The selected Windows SDK is not an adopted .vsconfig component.' }
    $requiredFiles = [Collections.Generic.List[string]]::new()
    foreach ($architecture in @('x86', 'x64', 'arm64')) {
        $requiredFiles.Add((Join-Path $properties.UCRTContentRoot "Lib/$($properties.TargetUniversalCRTVersion)/ucrt/$architecture/ucrt.lib"))
        $requiredFiles.Add((Join-Path $properties.WindowsSdkDir "Lib/$sdkVersion/um/$architecture/kernel32.lib"))
    }
    $requiredFiles.Add((Join-Path $properties.WindowsSdkDir "Include/$sdkVersion/um/Windows.h"))
    $requiredFiles.Add((Join-Path $properties.UCRTContentRoot "Include/$($properties.TargetUniversalCRTVersion)/ucrt/stdio.h"))
    $requiredFiles.Add((Join-Path $properties.WindowsSdkDir "bin/$sdkVersion/x64/makeappx.exe"))
    [pscustomobject]@{componentId = $component; properties = $properties; missingFiles = @($requiredFiles | Where-Object { -not [IO.File]::Exists($_) }) }
}

$before = Get-NativeWindowsSdkPrerequisites -LogName sdk-before
$after = $before
if ($before.missingFiles.Count -gt 0) {
    $before.missingFiles | Set-Content -LiteralPath (Join-Path $output 'missing-sdk-inputs.txt') -Encoding utf8NoBOM
    # These documented runner observations prevent accidental local installation;
    # they are cooperative context checks, not an OS authorization boundary.
    if ($env:GITHUB_ACTIONS -cne 'true' -or $env:RUNNER_ENVIRONMENT -cne 'github-hosted' -or $env:RUNNER_OS -cne 'Windows') {
        throw 'Missing pinned Windows SDK inputs; automatic installation is restricted to an ephemeral GitHub-hosted Windows runner.'
    }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    if (-not [Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Hosted SDK setup requires the runner administrator token; no interactive elevation is attempted.'
    }
    $installer = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) 'Microsoft Visual Studio/Installer/setup.exe'
    if (-not [IO.File]::Exists($installer)) { throw 'The native Visual Studio Installer is absent.' }
    if (-not $PSCmdlet.ShouldProcess($toolchain.visualStudioInstallationPath, "Add only $($before.componentId) through Visual Studio Installer")) { return }
    # setup.exe does not accept --wait. PowerShell's documented -Wait observes
    # the process tree. The workflow separately applies the canonical
    # hostedPrerequisiteStep category from the qualification policy.
    # Native argument parsing receives one quoted installation path; no shell,
    # product update, forced close, extra workload or automatic restart is used.
    $arguments = 'modify --installPath "{0}" --channelId "{1}" --add "{2}" --quiet --norestart' -f `
        $toolchain.visualStudioInstallationPath, $toolchain.visualStudioChannelId, $before.componentId
    $process = Start-Process -FilePath $installer -ArgumentList $arguments -WorkingDirectory $root `
        -RedirectStandardOutput (Join-Path $output 'installer.stdout.log') -RedirectStandardError (Join-Path $output 'installer.stderr.log') `
        -WindowStyle Hidden -PassThru -Wait
    [IO.File]::WriteAllText((Join-Path $output 'installer-exit-code.txt'), [string]$process.ExitCode)
    if ($process.ExitCode -eq 3010) { throw 'Native SDK installation requires a reboot; qualification stops without restarting the runner.' }
    if ($process.ExitCode -ne 0) { throw "Native SDK installation failed with exit code $($process.ExitCode)." }
    $selectedAfter = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
    if ($selectedAfter.visualStudioInstanceId -cne $toolchain.visualStudioInstanceId -or $selectedAfter.vcToolsVersion -cne $toolchain.vcToolsVersion) {
        throw 'SDK installation changed the selected locked Visual Studio toolchain.'
    }
    $after = Get-NativeWindowsSdkPrerequisites -LogName sdk-after
}
if ($after.componentId -cne $before.componentId -or $after.missingFiles.Count -ne 0) {
    throw 'The selected SDK still lacks required native inputs; no retargeting or missing-library filtering is permitted.'
}
$after | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'sdk-prerequisites.json') -Encoding utf8NoBOM
Write-Output "PASS: native SDK inputs are present for $($after.componentId); no build or runtime qualification is implied."
