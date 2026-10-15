<# Validate the prebuilt drop before deployment. The caller must independently
   authenticate build-context.json (the workflow uses native gh attestation
   verification). Local hashes alone do not establish publisher/source trust.
   Returns the payload repository root; writes only a fresh diagnostic inventory.
   No build, package registration or desktop action occurs here. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DropDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$ExpectedSourceCommit,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$drop=[IO.Path]::GetFullPath($DropDirectory)
$working=[IO.Path]::GetFullPath($WorkingDirectory)
if (-not [IO.Directory]::Exists($drop)) {throw 'The prebuilt interactive drop is absent.'}
if (Test-Path -LiteralPath $working) {throw 'Interactive drop validation requires a fresh working directory.'}
foreach ($path in @($drop,$working)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Interactive drop validation refuses reparse-point inputs or output ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
$relative=[IO.Path]::GetRelativePath($drop,$working)
if (-not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
    -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)) {throw 'Interactive validation work must be outside the input drop.'}
if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($working))) {throw 'Interactive validation working parent must exist.'}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$contextPath=Join-Path $drop 'build-context.json'
$json=[IO.File]::ReadAllText($contextPath)
$context=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -RequiredRootValueKind Object -SourceDescription 'Prebuilt interactive input context' -AsHashtable
if (-not (Test-Json -Json $json -SchemaFile (Join-Path (Split-Path -Parent $PSScriptRoot) 'eng/interactive-test-drop.schema.json'))) {throw 'Interactive input context schema refused the drop.'}
if ($context.sourceCommit -cne $ExpectedSourceCommit) {throw 'Interactive input source commit differs from the selected candidate.'}
$inventory=Join-Path $drop 'payload.json'
if ((Get-FileHash -LiteralPath $inventory).Hash.ToLowerInvariant() -cne $context.payloadManifestSha256) {throw 'Interactive input inventory digest differs from its context.'}
[void][IO.Directory]::CreateDirectory($working)
$observed=Join-Path $working 'payload.json'
& (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot (Join-Path $drop 'payload') -OutputPath $observed | Out-Null
if ((Get-FileHash -LiteralPath $observed).Hash.ToLowerInvariant() -cne $context.payloadManifestSha256) {
    throw 'Interactive input bytes differ from the authenticated inventory.'
}
# Only these producer-owned binary roots are consumed. The native inventory
# already excluded links and normalized relative paths before this scope check.
$files=(ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText($observed)) `
    -RequiredRootValueKind Object -SourceDescription 'Observed interactive payload inventory' -AsHashtable).files
$required=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($file in $files) {
    if (-not $file.relativePath.StartsWith('artifacts/bin/x64/Release/JpgSpinner.App/',[StringComparison]::Ordinal) -and
        -not $file.relativePath.StartsWith('artifacts/bin/x64/Release/JpgSpinner.Presentation.Tests/',[StringComparison]::Ordinal)) {
        throw 'Interactive payload contains files outside the approved binary roots.'
    }
    [void]$required.Add($file.relativePath)
}
foreach ($relative in @('JpgSpinner.App/AppxManifest.xml','JpgSpinner.App/JpgSpinner.exe','JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe')) {
    if (-not $required.Contains("artifacts/bin/x64/Release/$relative")) {throw 'Interactive payload omits an application/test prerequisite.'}
}
[pscustomobject]@{RepositoryRoot=(Join-Path $drop 'payload');SourceCommit=$context.sourceCommit;PayloadManifestSha256=$context.payloadManifestSha256}
