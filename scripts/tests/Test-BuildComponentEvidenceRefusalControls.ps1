<# Deliberately malformed lock/snapshot inputs must never become published
   build-context evidence. These are JSON fixtures, not a dependency resolver. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][hashtable]$NativeEvidenceDirectories,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer=Join-Path $repositoryRoot 'scripts/New-JpgSpinnerBuildComponentEvidence.ps1'
$lock=Join-Path $repositoryRoot 'src/JpgSpinner.App/packages.lock.json'
$original=[IO.File]::ReadAllText($lock)
$fixture=Join-Path $OwnedOutputRoot ('build-component-controls-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
foreach ($case in @(
    @{Name='unsupported-version';Diagnostic='unsupported shape';Mutate={param($doc) $doc.version=1}},
    @{Name='string-version';Diagnostic='unsupported shape';Mutate={param($doc) $doc.version='2'}},
    @{Name='array-target';Diagnostic='target is not an object';Mutate={param($doc) $doc.dependencies['native,Version=v0.0']=@()}},
    @{Name='unknown-dependency-kind';Diagnostic='supported resolved package identity';Mutate={param($doc) $doc.dependencies['native,Version=v0.0']['Microsoft.WindowsAppSDK'].type='Runtime'}},
    @{Name='non-string-version';Diagnostic='supported resolved package identity';Mutate={param($doc) $doc.dependencies['native,Version=v0.0']['Microsoft.WindowsAppSDK'].resolved=25}},
    @{Name='missing-content-hash';Diagnostic='supported resolved package identity';Mutate={param($doc) $doc.dependencies['native,Version=v0.0']['Microsoft.WindowsAppSDK'].Remove('contentHash')}},
    @{Name='inconsistent-content';Diagnostic='disagrees across lock targets';Mutate={param($doc)
        $doc.dependencies['native,Version=v0.0/win-x64']['Microsoft.Web.WebView2'].contentHash='inconsistent-fixture-hash'
    }}
)) {
    $document=$original | ConvertFrom-Json -AsHashtable
    & $case.Mutate $document | Out-Null
    $path=Join-Path $fixture "$($case.Name).json"
    $document | ConvertTo-Json -Depth 50 | Out-File -LiteralPath $path -Encoding utf8NoBOM -NoClobber
    $work=Join-Path $fixture "$($case.Name)-work"
    $output=Join-Path $fixture "$($case.Name)-output"
    $message=$null
    try {& $producer -NativeEvidenceDirectories $NativeEvidenceDirectories -NuGetLockPath $path -OutputDirectory $output -WorkingDirectory $work | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal) -or
        (Test-Path -LiteralPath $work) -or (Test-Path -LiteralPath $output)) {
        throw "NuGet control '$($case.Name)' was not refused before output effects: $message"
    }
}
foreach ($case in @(
    @{Name='array-root';Json='[]';Diagnostic='root must be a JSON object'},
    @{Name='duplicate-root-member';Json=$original.Insert($original.IndexOf('{')+1,'"version":2,');Diagnostic='not valid strict JSON'}
)) {
    $path=Join-Path $fixture "$($case.Name).json"
    [IO.File]::WriteAllText($path,$case.Json)
    $work=Join-Path $fixture "$($case.Name)-work"
    $output=Join-Path $fixture "$($case.Name)-output"
    $message=$null
    try {& $producer -NativeEvidenceDirectories $NativeEvidenceDirectories -NuGetLockPath $path -OutputDirectory $output -WorkingDirectory $work | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal) -or
        (Test-Path -LiteralPath $work) -or (Test-Path -LiteralPath $output)) {
        throw "Raw JSON control '$($case.Name)' did not refuse before conversion/output: $message"
    }
}
$missing=$NativeEvidenceDirectories.Clone()
$missing.Remove('x86')
$message=$null
try {& $producer -NativeEvidenceDirectories $missing -NuGetLockPath $lock -OutputDirectory (Join-Path $fixture 'missing-output') -WorkingDirectory (Join-Path $fixture 'missing-work') | Out-Null}
catch {$message=$_.Exception.Message}
if ($message -cne 'Build-component evidence requires exactly arm64, x64 and x86 native snapshots.') {throw "Missing architecture was not refused: $message"}

# Copy only the caller's small native evidence, never the installed binaries.
# Removing one transitive metadata document must refuse publication even though
# both application roots and the producer's required contract still exist.
$incomplete=Join-Path $fixture 'incomplete-native'
[void][IO.Directory]::CreateDirectory($incomplete)
foreach ($file in Get-ChildItem -LiteralPath $NativeEvidenceDirectories.x86 -File -Recurse) {
    $relative=[IO.Path]::GetRelativePath($NativeEvidenceDirectories.x86,$file.FullName)
    if ($relative.Replace('\','/') -ceq 'installed/x86-windows-static-md/share/zlib/vcpkg.spdx.json') {continue}
    $copy=Join-Path $incomplete $relative
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copy))
    [IO.File]::Copy($file.FullName,$copy,$false)
}
$missingMetadata=$NativeEvidenceDirectories.Clone()
$missingMetadata.x86=$incomplete
$message=$null
try {& $producer -NativeEvidenceDirectories $missingMetadata -NuGetLockPath $lock -OutputDirectory (Join-Path $fixture 'incomplete-output') -WorkingDirectory (Join-Path $fixture 'incomplete-work') | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('architecture-specific metadata snapshot',[StringComparison]::Ordinal) -or
    (Test-Path -LiteralPath (Join-Path $fixture 'incomplete-output'))) {throw "Missing transitive native metadata was not refused: $message"}
Write-Output 'PASS: malformed/duplicate/non-object NuGet inputs, inconsistent target identities, missing architecture and missing transitive metadata refuse publication.'
