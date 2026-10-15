<# Native configuration qualification through the public error-origin path.
   Explicit Directory.Build paths reuse the real root policy from two fresh
   owned source directories. A qualified existing projection is read only;
   there is no package restore, vendor patch or undocumented compiler switch. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$WinrtProjectionDirectory,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not [IO.File]::Exists((Join-Path $WinrtProjectionDirectory winrt/base.h))) {throw 'The qualified native projection is absent.'}
$fixture=Join-Path $OwnedOutputRoot ('winrt-error-origin-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$toolchain=& (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
Import-Module (Join-Path $root scripts/ArtifactToolProcess.psm1) -Force
$images=[Collections.Generic.List[string]]::new()
foreach ($lane in @(@{Name='first';Configuration='Release'},@{Name='second';Configuration='Release'},@{Name='debug';Configuration='Debug'})) {
    $directory=Join-Path $fixture $lane.Name
    Copy-Item -LiteralPath (Join-Path $root eng/WinrtErrorSourceLocationProbe) -Destination $directory -Recurse
    $output=Join-Path $directory bin
    Invoke-ArtifactToolProcess -Executable $toolchain.msBuildExecutablePath -WorkingDirectory $directory -LogDirectory $directory -LogName build -TimeoutSeconds 120 -Arguments @(
        (Join-Path $directory WinrtErrorSourceLocationProbe.vcxproj),'-nologo','-noAutoResponse','-m:2','-t:Build',
        "-p:Configuration=$($lane.Configuration)",'-p:Platform=x64',
        "-p:DirectoryBuildPropsPath=$(Join-Path $root Directory.Build.props)",
        "-p:DirectoryBuildTargetsPath=$(Join-Path $root Directory.Build.targets)",
        "-p:WinrtProjectionDirectory=$WinrtProjectionDirectory",
        "-p:OutDir=$output/","-p:IntDir=$(Join-Path $directory obj)/"
    ) | Out-Null
    $image=Join-Path $output WinrtErrorSourceLocationProbe.exe
    & $image
    if ($LASTEXITCODE -ne 0) {throw "Native $($lane.Configuration) error-origin policy failed with exit $LASTEXITCODE; retain $directory"}
    if ($lane.Configuration -ceq 'Release') {$images.Add($image)}
}
if ((Get-FileHash -LiteralPath $images[0]).Hash -cne (Get-FileHash -LiteralPath $images[1]).Hash) {throw 'Native Release error-origin probes still differ across independent source roots.'}
Write-Output "PASS: Release omits optional source origins with unchanged HRESULTs and cross-root executable bytes; Debug retains origins. Evidence: $fixture"
