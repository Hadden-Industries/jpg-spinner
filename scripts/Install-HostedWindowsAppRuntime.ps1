<# Deploy the adopted Windows App SDK runtime only on an ephemeral hosted test
   runner. Microsoft's native installer owns package applicability, ordering and
   existing-version handling. SDK restoration/compilation alone cannot satisfy
   the unpackaged presentation host's bootstrap dependency.
   https://learn.microsoft.com/windows/apps/windows-app-sdk/deploy-unpackaged-apps
   https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads #>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$InstallerPath,
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
# WhatIf can inspect an already acquired binary locally, but never acquires or
# installs anything. Environment values are cooperative context checks, not an
# OS authorization boundary. No interactive elevation or workstation fallback.
$hosted = $env:GITHUB_ACTIONS -ceq 'true' -and $env:RUNNER_ENVIRONMENT -ceq 'github-hosted' -and $env:RUNNER_OS -ceq 'Windows'
if (-not $WhatIfPreference -and -not $hosted) {
    throw 'Windows App Runtime installation is restricted to an ephemeral GitHub-hosted Windows runner.'
}
if ($WhatIfPreference -and [string]::IsNullOrWhiteSpace($InstallerPath)) {
    throw 'Local WhatIf qualification requires an existing installer; acquisition is not permitted.'
}
if (-not $WhatIfPreference) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    if (-not [Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Hosted runtime setup requires the runner administrator token; no interactive elevation is attempted.'
    }
}
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Runtime prerequisite diagnostics require fresh output.' }
[void][IO.Directory]::CreateDirectory($output)
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$context = @{WorkingDirectory = $root; LogDirectory = $output }
# Native evaluation consumes the adopted central package version without a
# parallel XML/lock-file parser or an SDK upgrade hidden in runner setup. The
# deployment lane needs no C++ toolchain/project imports: evaluate the central
# MSBuild document with the installed .NET SDK, without running any build target.
$dotnet = (Get-Command dotnet.exe -CommandType Application -ErrorAction Stop).Source
$json = Invoke-ArtifactToolProcess @context -Executable $dotnet -LogName sdk-package-selection `
    -EnvironmentValues @{DOTNET_NOLOGO = '1' } -Arguments @(
    'msbuild', (Join-Path $root 'Directory.Packages.props'),
    '-nologo', '-noAutoResponse', '-nodeReuse:false', '-maxCpuCount:1',
    '-p:Configuration=Release', '-p:Platform=x64', '-getItem:PackageVersion'
)
$selection = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -RequiredRootValueKind Object -SourceDescription 'Native Windows App SDK package selection' -AsHashtable
$sdkPackages = @($selection.Items.PackageVersion | Where-Object { $_.Identity -ceq 'Microsoft.WindowsAppSDK' })
$windowsAppSdkPackageVersion = '2.5.1'
if ($sdkPackages.Count -ne 1 -or $sdkPackages[0].Version -cne $windowsAppSdkPackageVersion) {
    throw 'Runtime installer pins do not match the unique adopted Microsoft.WindowsAppSDK package version.'
}
# These immutable Microsoft download endpoints are the measured targets of the
# official 2.5.1 release links. Hashes bind the exact associated native binaries;
# their PE FileVersion is 2.5, distinct from the SDK package version 2.5.1.
$pins = @{
    x64   = @{
        uri    = 'https://download.microsoft.com/download/de664922-b046-432f-bb2a-54aec3c5f1f6/WindowsAppRuntimeInstall-x64.exe'
        sha256 = '931a421e8dc3e6e67724806cb67fecdbb88dfe323f0170842eb4a4b4b149f1e2'
    }
    x86   = @{
        uri    = 'https://download.microsoft.com/download/652efa1f-ce5b-4ce9-8bfe-5a08e96145f4/WindowsAppRuntimeInstall-x86.exe'
        sha256 = '76dbd7c272cee0bf18f0b7228255b353d669cd55dc530209c45646f47acb89d4'
    }
    ARM64 = @{
        uri    = 'https://download.microsoft.com/download/83fdef97-1ca8-491d-80f3-a94b5adc5b1c/WindowsAppRuntimeInstall-arm64.exe'
        sha256 = 'd5e4d34547eb4e31c64bf1532415b3019c0d92b750e72eb18d3d95bd00feacbb'
    }
}
$pin = $pins[$Architecture]
$downloaded = [string]::IsNullOrWhiteSpace($InstallerPath)
if ($downloaded) {
    # Do not upload the reconstructible 120 MB installer with the small native
    # diagnostics. Only this UUID-owned hosted temporary file is disposable.
    $InstallerPath = Join-Path $env:RUNNER_TEMP ('jpg-spinner-runtime-' + [Guid]::NewGuid().ToString('N') + '.exe')
    Invoke-WebRequest -Uri $pin.uri -OutFile $InstallerPath -TimeoutSec (Get-QualificationTimeout -Category artifactDownload) -MaximumRedirection 0
}
$installer = (Get-Item -LiteralPath $InstallerPath -ErrorAction Stop).FullName
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -cne $pin.sha256) { throw 'Windows App Runtime installer SHA-256 differs from the adopted release binary.' }
$signature = Get-AuthenticodeSignature -LiteralPath $installer
if ($signature.Status -ne 'Valid' -or
    $signature.SignerCertificate.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne 'Microsoft Corporation') {
    throw 'Windows App Runtime installer lacks a valid Microsoft Corporation signature.'
}
$fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($installer).FileVersion
$installerIdentity = @{windowsAppSdkPackageVersion = $windowsAppSdkPackageVersion; architecture = $Architecture; uri = $pin.uri;
    sha256 = $hash; fileVersion = $fileVersion; signatureStatus = [string]$signature.Status
} |
    ConvertTo-Json
# WhatIf suppresses deployment, while this explicitly requested diagnostic is
# still retained alongside the read-only native selection streams.
[IO.File]::WriteAllText((Join-Path $output 'runtime-installer-identity.json'), $installerIdentity)
if (-not $PSCmdlet.ShouldProcess('Ephemeral hosted test runner', "Deploy Windows App SDK $windowsAppSdkPackageVersion runtime through the native $Architecture installer")) { return }
# Do not force shutdowns, ignore errors, implement a package-graph solver, or
# treat installation as evidence that the following real test host passed.
[void](Invoke-ArtifactToolProcess @context -Executable $installer -Arguments @('--quiet') -LogName runtime-install -TimeoutSeconds (Get-QualificationTimeout -Category runtimeInstallation))
[IO.File]::WriteAllText((Join-Path $output 'installer-exit-code.txt'), '0')
if ($downloaded) { Remove-Item -LiteralPath $installer -ErrorAction Stop }
Write-Output 'PASS: native runtime deployment completed; the following tests must independently establish host execution.'
