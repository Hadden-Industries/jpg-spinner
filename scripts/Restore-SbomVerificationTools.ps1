<# Restore the reviewed native vcpkg/SPDX verification inputs from caller-owned
   downloads into one fresh ordinary external directory. The caller owns tool
   acquisition and Node selection; no system installation, version resolution,
   product compilation, global setting, cache repair or release action occurs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DownloadDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$NodeExecutablePath,
    [Parameter(Mandatory)][string]$NpmCliPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$downloads=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($DownloadDirectory)
$output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
foreach($path in @($downloads,$output)) {
    $ancestor=$path
    while(-not [string]::IsNullOrEmpty($ancestor)) {
        if((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'SBOM tool restoration refuses redirected roots.'}
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
function Test-Within([string]$Root,[string]$Path) {
    $relative=[IO.Path]::GetRelativePath($Root,$Path)
    return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
        -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
}
if((Test-Path -LiteralPath $output) -or -not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($output)) -or
    (Test-Within $root $output) -or (Test-Within $output $root) -or
    (Test-Within $downloads $output) -or (Test-Within $output $downloads)) {
    throw 'SBOM tool restoration requires a fresh external output directory.'
}
Import-Module (Join-Path $PSScriptRoot JsonObjectMemberValidation.psm1) -Force
$pins=ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $root eng/sbom-tools.json) -Raw) -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
# Authenticate all original inputs before extraction or npm execution. These
# identities are the single reviewed authority, not remote version resolution.
$assets=@(
    @{Name='vcpkg.exe';Hash=$pins.vcpkgExporter.executableSha256},
    @{Name='vcpkg-standalone-bundle.tar.gz';Hash=$pins.vcpkgExporter.bundleSha256},
    @{Name='spdx-json-schema.json';Hash=$pins.spdxSchema.sha256},
    @{Name='tools-java.zip';Hash=$pins.spdxWorkgroupTools.archiveSha256}
)
$handles=[Collections.Generic.List[IDisposable]]::new()
try {
    foreach($asset in $assets) {
        $path=Join-Path $downloads $asset.Name
        if(-not [IO.File]::Exists($path) -or ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'SBOM verification download is absent or redirected.'}
        # Keep each authenticated archive open read-only for the entire restore;
        # quiescent caller-owned inputs may not change after digest validation.
        $handle=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        $handles.Add($handle)
        if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($handle)).ToLowerInvariant() -cne $asset.Hash){throw 'SBOM verification download differs from its reviewed digest.'}
    }
    [void][IO.Directory]::CreateDirectory($output)
    $vcpkg=Join-Path $output vcpkg
    [void][IO.Directory]::CreateDirectory($vcpkg)
    Import-Module (Join-Path $PSScriptRoot ArtifactToolProcess.psm1) -Force
    $tar=(Get-Command tar -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    [void](Invoke-ArtifactToolProcess -Executable $tar -WorkingDirectory $output -LogDirectory $output -LogName vcpkg-extraction `
        -Arguments @('-xzf',(Join-Path $downloads vcpkg-standalone-bundle.tar.gz),'-C',$vcpkg))
    [IO.File]::Copy((Join-Path $downloads vcpkg.exe),(Join-Path $vcpkg vcpkg.exe),$false)
    $spdx=Join-Path $output spdx
    [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $downloads tools-java.zip),$spdx)
    $jar=Join-Path $spdx "tools-java-$($pins.spdxWorkgroupTools.version)-jar-with-dependencies.jar"
    if((Get-FileHash -LiteralPath $jar).Hash.ToLowerInvariant() -cne $pins.spdxWorkgroupTools.jarSha256){throw 'Restored SPDX verifier differs from its reviewed digest.'}
    $schema=Join-Path $output spdx-json-schema.json
    [IO.File]::Copy((Join-Path $downloads spdx-json-schema.json),$schema,$false)
    $validator=Join-Path $output schema-validator
    [void][IO.Directory]::CreateDirectory($validator)
    foreach($name in @('package.json','package-lock.json')) {
        [IO.File]::Copy((Join-Path $root "eng/spdx-validation/$name"),(Join-Path $validator $name),$false)
    }
    # Invoke the selected npm CLI through native Node, not a .cmd shell grammar.
    # npm owns lock/integrity semantics; lifecycle scripts and audit are disabled.
    [void](Invoke-ArtifactToolProcess -Executable $NodeExecutablePath -WorkingDirectory $validator -LogDirectory $output -LogName schema-validator-restore `
        -Arguments @($NpmCliPath,'ci','--ignore-scripts','--no-audit','--no-fund'))
    foreach($name in @('package.json','package-lock.json')) {
        if((Get-FileHash -LiteralPath (Join-Path $root "eng/spdx-validation/$name")).Hash -cne
            (Get-FileHash -LiteralPath (Join-Path $validator $name)).Hash){throw 'Native validator restore changed a source or staged lock.'}
    }
    [pscustomobject]@{VcpkgRoot=$vcpkg;SpdxSchemaPath=$schema;SpdxToolsJarPath=$jar;SchemaValidatorToolsDirectory=$validator}
} finally {foreach($handle in $handles){$handle.Dispose()}}
