<# Observe the selected native toolchain and committed dependency authorities.
   This is an input/environment record, never a claim that a build or tests ran.
   Paths retain their full rooted scope using stable installation placeholders. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
    [Parameter(Mandatory)][ValidateSet('x86','x64','ARM64')][string]$Architecture,
    [Parameter(Mandatory)][string]$OutputPath,
    [switch]$RequireCleanSourceTree
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
if (Test-Path -LiteralPath $output) { throw 'Build provenance output must be fresh and absent.' }
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$architectureName = if ($Architecture -ieq 'ARM64') { 'ARM64' } else { $Architecture.ToLowerInvariant() }
$configurationName = if ($Configuration -ieq 'Debug') { 'Debug' } else { 'Release' }
$platform = if ($architectureName -ceq 'x86') { 'Win32' } else { $architectureName }
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
Push-Location -LiteralPath $root
try {
    $propertiesJson = & $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
        -nologo -noAutoResponse "-p:Configuration=$configurationName" "-p:Platform=$platform" `
        -p:GenerateAppxPackageOnBuild=true '-getProperty:VCToolsVersion,WindowsTargetPlatformVersion,MakeAppxExeFullPath,NuGetPackageRoot'
    if ($LASTEXITCODE -ne 0) { throw 'Native MSBuild provenance property observation failed.' }
    $properties = (ConvertFrom-JsonWithUniqueObjectMembers -JsonText ($propertiesJson -join "`n") `
        -SourceDescription 'Native MSBuild provenance properties' -RequiredRootValueKind Object -AsHashtable).Properties
    $sourceCommit = & git -C $root rev-parse --verify HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Source commit observation failed.' }
    $sourceStatus = @(& git -C $root status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw 'Source worktree observation failed.' }
    $sourceTreeIsClean = $sourceStatus.Count -eq 0
    if ($RequireCleanSourceTree -and -not $sourceTreeIsClean) { throw 'Build provenance requires a clean source worktree.' }
    $lockPaths = & git -C $root ls-files -z -- 'src/**/packages.lock.json' 'tests/**/packages.lock.json'
    if ($LASTEXITCODE -ne 0) { throw 'Committed NuGet lock inventory observation failed.' }
}
finally { Pop-Location }
$pathRoots = [ordered]@{
    '<visual-studio>' = $toolchain.visualStudioInstallationPath
    '<nuget-packages>' = $properties.NuGetPackageRoot
    '<windows-sdk>' = (Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)) 'Windows Kits/10')
}
function Get-SelectedExecutableObservation {
    param([string]$ExecutablePath)
    $canonical = [IO.Path]::GetFullPath($ExecutablePath)
    $normalized = $null
    foreach ($entry in $pathRoots.GetEnumerator()) {
        if ([string]::IsNullOrWhiteSpace($entry.Value)) { continue }
        $relative = [IO.Path]::GetRelativePath($entry.Value,$canonical)
        if (-not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
            -not $relative.StartsWith('..\',[StringComparison]::Ordinal)) {
            $normalized = $entry.Key + '/' + $relative.Replace('\','/')
            break
        }
    }
    if ($null -eq $normalized) { throw 'Selected executable cannot be represented without leaking an unrecognized host path.' }
    return [ordered]@{
        path=$normalized
        fileVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($canonical).FileVersion
        sha256=(Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$nugetLocks = [Collections.Generic.SortedDictionary[string,string]]::new([StringComparer]::Ordinal)
foreach ($relativePath in (($lockPaths -join '') -split "`0")) {
    if ([string]::IsNullOrWhiteSpace($relativePath)) { continue }
    $absolutePath = Join-Path $root $relativePath
    # Only actual modern project locks belong to this inventory. A source
    # fixture containing packages.lock.json is not another build project.
    $projects = @(Get-ChildItem -LiteralPath (Split-Path -Parent $absolutePath) -Filter '*.vcxproj' -File)
    if ($projects.Count -eq 0) { continue }
    $nugetLocks.Add($relativePath.Replace('\','/'),(Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash.ToLowerInvariant())
}
$manifest = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $root 'vcpkg.json') -Raw) `
    -SourceDescription 'vcpkg manifest' -RequiredRootValueKind Object -AsHashtable
$actionPins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $root 'eng/github-actions-lock.json') -Raw) `
    -SourceDescription 'GitHub action pins' -RequiredRootValueKind Object -AsHashtable
$actionCommits = [ordered]@{}
foreach ($entry in $actionPins.actions.GetEnumerator()) { $actionCommits[$entry.Key] = $entry.Value.commit }
$isHosted = [Environment]::GetEnvironmentVariable('GITHUB_ACTIONS') -ceq 'true'
$runnerImage = if ($isHosted) {
    $imageVersion = [Environment]::GetEnvironmentVariable('ImageVersion')
    if ([string]::IsNullOrWhiteSpace($imageVersion)) { throw 'Hosted provenance requires the actual runner ImageVersion.' }
    @{label='windows-2025-vs2026';version=$imageVersion}
} else { @{label='local-development';version='not-a-hosted-runner'} }
$record = [ordered]@{
    schemaVersion=1;sourceCommit=$sourceCommit;sourceTreeIsClean=$sourceTreeIsClean
    architecture=$architectureName;configuration=$configurationName;runnerImage=$runnerImage
    operatingSystemVersion=[Environment]::OSVersion.Version.ToString()
    vcToolsVersion=$properties.VCToolsVersion;windowsSdkVersion=$properties.WindowsTargetPlatformVersion
    selectedTools=[ordered]@{
        msBuild=(Get-SelectedExecutableObservation $toolchain.msBuildExecutablePath)
        compiler=(Get-SelectedExecutableObservation $toolchain.targetTools[$architectureName.ToLowerInvariant()].compilerExecutablePath)
        linker=(Get-SelectedExecutableObservation $toolchain.targetTools[$architectureName.ToLowerInvariant()].linkerExecutablePath)
        makeAppx=(Get-SelectedExecutableObservation $properties.MakeAppxExeFullPath)
    }
    nugetLocks=$nugetLocks;vcpkgBaseline=$manifest['builtin-baseline']
    vcpkgManifestSha256=(Get-FileHash -LiteralPath (Join-Path $root 'vcpkg.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    actionCommits=$actionCommits
}
# Exclusive creation preserves earlier observations. Native schema validation
# below may leave a failed record for diagnosis; it never certifies execution.
$bytes = [Text.UTF8Encoding]::new($false).GetBytes(($record | ConvertTo-Json -Depth 10) + "`n")
$stream = [IO.FileStream]::new($output,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
& (Join-Path $PSScriptRoot 'Test-BuildProvenance.ps1') -Path $output
Write-Output $output
