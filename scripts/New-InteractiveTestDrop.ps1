<# Copy already-built x64 Release application/test inputs for a deployment-only
   interactive lane. This does not build, sign, install, certify source provenance
   or run tests. The caller owns a frozen binary tree and its source-commit claim.
   Publish only a complete fresh directory; retain failed working copies. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BinaryDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$binary=[IO.Path]::GetFullPath($BinaryDirectory)
$output=[IO.Path]::GetFullPath($OutputDirectory)
$working=[IO.Path]::GetFullPath($WorkingDirectory)
if (-not [IO.Directory]::Exists($binary)) {throw 'The prebuilt x64 Release binary directory is absent.'}
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $working)) {throw 'Interactive drop destinations must be fresh and absent.'}
foreach ($path in @($binary,$output,$working)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Interactive drop refuses reparse-point roots or output ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
function Test-PathWithin([string]$Root,[string]$Path) {
    $relative=[IO.Path]::GetRelativePath($Root,$Path)
    return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
        -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
}
foreach ($destination in @($working,$output)) {
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {throw 'Interactive drop destination parents must exist.'}
    if ((Test-PathWithin $binary $destination) -or (Test-PathWithin $destination $binary)) {throw 'Interactive drop destinations overlap binary inputs.'}
}
if ((Test-PathWithin $working $output) -or (Test-PathWithin $output $working) -or
    -not [string]::Equals([IO.Path]::GetPathRoot($working),[IO.Path]::GetPathRoot($output),[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Interactive drop destinations must be disjoint and on the same volume.'
}
foreach ($relative in @('JpgSpinner.App/AppxManifest.xml','JpgSpinner.App/JpgSpinner.exe','JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe')) {
    if (-not [IO.File]::Exists((Join-Path $binary $relative))) {throw "Prebuilt interactive prerequisite is absent: $relative"}
}
[void][IO.Directory]::CreateDirectory($working)
$candidate=Join-Path $working 'candidate'
$payload=Join-Path $candidate 'payload/artifacts/bin/x64/Release'
[void][IO.Directory]::CreateDirectory($payload)
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
foreach ($directory in @('JpgSpinner.App','JpgSpinner.Presentation.Tests')) {
    # Reuse the non-following ordinal inventory to choose ordinary input files.
    # Copy all native assets, including symbols; no deployment dependency list
    # is guessed by extension. Only the two application/test roots are shipped.
    $source=Join-Path $binary $directory
    $before=Join-Path $working "$directory-before.json"
    & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $source -OutputPath $before | Out-Null
    $manifest=ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText($before)) `
        -RequiredRootValueKind Object -SourceDescription 'Interactive binary inventory' -AsHashtable
    $copy=Join-Path $payload $directory
    [void][IO.Directory]::CreateDirectory($copy)
    foreach ($file in $manifest.files) {
        $destination=Join-Path $copy $file.relativePath
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        [IO.File]::Copy((Join-Path $source $file.relativePath),$destination,$false)
    }
    $after=Join-Path $working "$directory-after.json"
    & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $source -OutputPath $after | Out-Null
    $copied=Join-Path $working "$directory-copy.json"
    & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $copy -OutputPath $copied | Out-Null
    $hash=(Get-FileHash -LiteralPath $before).Hash
    if ($hash -cne (Get-FileHash -LiteralPath $after).Hash -or $hash -cne (Get-FileHash -LiteralPath $copied).Hash) {throw 'Interactive binary inputs changed during copying.'}
}
$inventory=Join-Path $candidate 'payload.json'
& (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot (Join-Path $candidate 'payload') -OutputPath $inventory | Out-Null
$metadata=@{schemaVersion=1;sourceCommit=$SourceCommit;architecture='x64';configuration='Release';payloadManifestSha256=(Get-FileHash -LiteralPath $inventory).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $candidate 'build-context.json'),($metadata | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
& (Join-Path $PSScriptRoot 'Test-InteractiveTestDrop.ps1') -DropDirectory $candidate -ExpectedSourceCommit $SourceCommit `
    -WorkingDirectory (Join-Path $working 'validation') | Out-Null
[IO.Directory]::Move($candidate,$output)
Write-Output $output
