<# Generate one independently validated native SPDX artifact document.
   Caller owns frozen ordinary payload/component snapshots, explicit cached Go
   dependencies and fresh external working/output paths. No extraction, signing,
   install, graph resolution or release approval occurs here. Failed candidates
   stay in WorkingDirectory and never occupy the requested published path. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PayloadRoot,
    [Parameter(Mandatory)][string]$ComponentEvidenceRoot,
    [Parameter(Mandatory)][string]$ComponentContractPath,
    [Parameter(Mandatory)][string]$PublicArtifactName,
    [Parameter(Mandatory)][string]$PublicArtifactVersion,
    [Parameter(Mandatory)][string]$OutputPath,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$BuildCacheDirectory,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SchemaValidatorToolsDirectory,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$payload = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PayloadRoot)
$components = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ComponentEvidenceRoot)
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$working = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
if (-not [IO.Directory]::Exists($payload) -or -not [IO.Directory]::Exists($components)) { throw 'Artifact inventory input directories are absent.' }
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $working)) { throw 'Artifact generation output and working directory must be fresh and absent.' }
# Enforce ordinary, quiescent snapshot/output boundaries before native tools or
# directory creation. A fresh leaf does not exclude an ancestor junction that
# redirects an apparently external destination back into an inventory tree.
foreach ($path in @($payload,$components,$output,$working)) {
    $ancestor = $path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Artifact generation refuses reparse-point inputs or output ancestors.'
        }
        $ancestor = [IO.Path]::GetDirectoryName($ancestor)
    }
}
foreach ($inputRoot in @($payload,$components)) {
    foreach ($destination in @($working,$output)) {
        $relative = [IO.Path]::GetRelativePath($inputRoot,$destination)
        if (-not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
            -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)) {
            throw 'Artifact generation outputs must be outside inventory inputs.'
        }
    }
}
foreach ($parent in @([IO.Path]::GetDirectoryName($working),[IO.Path]::GetDirectoryName($output))) {
    if (-not [IO.Directory]::Exists($parent)) { throw 'Artifact generation requires existing owned output parents.' }
}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $repositoryRoot 'eng/sbom-tools.json') -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
foreach ($validator in @(@{Path=$SpdxSchemaPath;Hash=$pins.spdxSchema.sha256},@{Path=$SpdxToolsJarPath;Hash=$pins.spdxWorkgroupTools.jarSha256})) {
    if ((Get-FileHash -LiteralPath $validator.Path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $validator.Hash) {
        throw 'SPDX validator identity differs from the reviewed pin.'
    }
}
[void][IO.Directory]::CreateDirectory($working)

Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force

$settings = @{
    GOMODCACHE=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ModuleCacheDirectory)
    GOCACHE=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildCacheDirectory)
    GOMAXPROCS='2';GOTOOLCHAIN='local';GOWORK='off';GOENV='off';GOFLAGS='';GOPROXY='off'
}
$toolContext=@{WorkingDirectory=(Join-Path $repositoryRoot 'eng/artifact-sbom');LogDirectory=$working;EnvironmentValues=$settings}
$moduleJson = Invoke-ArtifactToolProcess @toolContext -Executable $GoExecutablePath -Arguments @('list','-mod=readonly','-m','-json',$pins.syftSdk.module) -LogName sdk-identity
$sdk = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $moduleJson -RequiredRootValueKind Object -SourceDescription 'Native Syft module identity' -AsHashtable
if ($sdk.Version -cne $pins.syftSdk.version -or $sdk.Sum -cne $pins.syftSdk.moduleSum -or $sdk.Contains('Replace')) {
    throw 'Native Syft SDK identity differs from the reviewed pin.'
}
$moduleDefinitionJson = Invoke-ArtifactToolProcess @toolContext -Executable $GoExecutablePath -Arguments @('mod','edit','-json') -LogName module-definition
$moduleDefinition = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $moduleDefinitionJson -RequiredRootValueKind Object -SourceDescription 'Native Go module definition' -AsHashtable
if ($moduleDefinition.Contains('Replace') -and $null -ne $moduleDefinition.Replace -and $moduleDefinition.Replace.Count -ne 0) {
    throw 'Artifact tool module replacements are forbidden.'
}
# Verify every cached filename literally, including upstream test fixtures
# ending in spaces. Win32's extended namespace prevents normalization.
# Keep this prefix out of the build: Go embed globs interpret its '?' as a
# pattern character. Both native operations still use the same cache tree.
# https://learn.microsoft.com/windows/win32/fileio/naming-a-file
$verificationCache = $settings.GOMODCACHE
if ($IsWindows -and -not $verificationCache.StartsWith('\\?\',[StringComparison]::Ordinal)) {
    $verificationCache = if ($verificationCache.StartsWith('\\',[StringComparison]::Ordinal)) {
        '\\?\UNC\' + $verificationCache.Substring(2)
    } else { '\\?\' + $verificationCache }
}
[void](Invoke-ArtifactToolProcess @toolContext -Executable $GoExecutablePath -Arguments @('mod','verify') -LogName module-integrity -EnvironmentOverrides @{GOMODCACHE=$verificationCache})
# Establish the independent byte inventory before asking the cataloger to
# describe it. The generator never supplies its own acceptance expectation.
$payloadManifest = Join-Path $working 'payload.json'
& (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $payload -OutputPath $payloadManifest | Out-Null
$executable = Join-Path $working 'artifact-sbom.exe'
[void](Invoke-ArtifactToolProcess @toolContext -Executable $GoExecutablePath -Arguments @('build','-mod=readonly','-p=2','-trimpath','-o',$executable,'.') -LogName sdk-build -TimeoutSeconds 600)
$candidate = Join-Path $working 'candidate.spdx.json'
[void](Invoke-ArtifactToolProcess @toolContext -Executable $executable -Arguments @('-payload',$payload,'-components',$components,'-name',$PublicArtifactName,'-version',$PublicArtifactVersion,'-output',$candidate) -LogName native-spdx)
# Successful serialization alone is insufficient. Reuse the official schema
# and semantic consumers plus the separate payload/component evidence gate.
# On refusal the candidate stays diagnostic, not a published release file.
& (Join-Path $PSScriptRoot 'Test-ArtifactSbom.ps1') -ManifestPath $candidate -PayloadManifestPath $payloadManifest `
    -ComponentContractPath $ComponentContractPath -PublicArtifactName $PublicArtifactName -PublicArtifactVersion $PublicArtifactVersion `
    -SpdxSchemaPath $SpdxSchemaPath -SchemaValidatorToolsDirectory $SchemaValidatorToolsDirectory -JavaExecutablePath $JavaExecutablePath -SpdxToolsJarPath $SpdxToolsJarPath |
    Out-File -LiteralPath (Join-Path $working 'validation.log') -Encoding utf8NoBOM
[IO.File]::Move($candidate,$output)
Write-Output $output
