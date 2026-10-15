<# Exercise the real root policy through native MSBuild, LINK and a headless
   executable. This is a configuration/identity control, not complete app-byte
   reproducibility. The retained frozen-input LINK experiment supplies that
   causal counterevidence; only a new whole pair can qualify the application. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('release-comdat-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$toolchain=& (Join-Path $root scripts/Resolve-MSBuildToolchain.ps1)
Import-Module (Join-Path $root scripts/ArtifactToolProcess.psm1) -Force
foreach($configuration in @('Release','Debug')) {
    $directory=Join-Path $fixture $configuration
    Copy-Item -LiteralPath (Join-Path $root eng/ReleaseComdatIdentityProbe) -Destination $directory -Recurse
    $output=Join-Path $directory bin
    [void](Invoke-ArtifactToolProcess -Executable $toolchain.msBuildExecutablePath -WorkingDirectory $directory -LogDirectory $directory -LogName build -TimeoutSeconds 120 `
        -Arguments @((Join-Path $directory ReleaseComdatIdentityProbe.vcxproj),'-nologo','-noAutoResponse','-m:2','-nodeReuse:false','-target:Build',
            "-p:Configuration=$configuration",'-p:Platform=x64',"-p:DirectoryBuildPropsPath=$(Join-Path $root Directory.Build.props)",
            "-p:DirectoryBuildTargetsPath=$(Join-Path $root Directory.Build.targets)","-p:OutDir=$output/","-p:IntDir=$(Join-Path $directory obj)/"))
    $image=Join-Path $output ReleaseComdatIdentityProbe.exe
    & $image
    if($LASTEXITCODE -ne 0){throw "Native $configuration COMDAT identity policy failed with exit $LASTEXITCODE; retain $directory"}
}
Write-Output "PASS: native x64 Release/Debug retain distinct COMDAT identities under actual root policy; whole application comparison remains separate. Evidence: $fixture"
