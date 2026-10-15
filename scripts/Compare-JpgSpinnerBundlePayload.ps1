<# Compare original three-architecture bundles after native SDK validation.
   Retain full raw manifests and hashes; this neither signs, installs nor grants
   release approval. Work is caller-owned, fresh, external and diagnostic on
   refusal. Never modify either input or substitute a rewritten artifact. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FirstBundlePath,
    [Parameter(Mandatory)][string]$SecondBundlePath,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$working=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
if (Test-Path -LiteralPath $working) {throw 'Bundle comparison requires a fresh absent working directory.'}
if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($working))) {throw 'Bundle comparison requires an existing owned parent.'}
$ancestor=$working
while (-not [string]::IsNullOrEmpty($ancestor)) {
    if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Bundle comparison refuses redirected output ancestors.'}
    $ancestor=[IO.Path]::GetDirectoryName($ancestor)
}
$handles=[Collections.Generic.List[IO.FileStream]]::new()
$builds=[Collections.Generic.List[object]]::new()
try {
    # Keep the original containers read-shared only through native extraction,
    # inventory and comparison. Expand owns path/identity/MakeAppx validation.
    foreach ($path in @($FirstBundlePath,$SecondBundlePath)) {
        $handles.Add([IO.File]::OpenRead($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($path)))
    }
    [void][IO.Directory]::CreateDirectory($working)
    foreach ($index in 0..1) {
        $name=@('first','second')[$index]
        $inspection=Join-Path $working $name
        $path=@($FirstBundlePath,$SecondBundlePath)[$index]
        & (Join-Path $PSScriptRoot 'Expand-JpgSpinnerBundle.ps1') -BundlePath $path -InspectionDirectory $inspection | Out-Null
        $manifest=Join-Path $working ($name+'-payload.json')
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot (Join-Path $inspection payloads) -OutputPath $manifest | Out-Null
        $handle=$handles[$index];$handle.Position=0
        $builds.Add([pscustomobject]@{
            Inspection=$inspection;Manifest=$manifest
            BundleSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($handle)).ToLowerInvariant()
            PayloadManifestSha256=(Get-FileHash -LiteralPath $manifest).Hash.ToLowerInvariant()
        })
    }
    $manifests=[Collections.Generic.List[object]]::new()
    foreach ($build in $builds) {
        # Consume only the freshly generated canonical native-writer inventory,
        # not caller-supplied JSON. Exact ordinal path sets avoid wildcard or
        # dependency-WinMD exclusions. The platform JSON reader owns syntax.
        $document=[Text.Json.JsonDocument]::Parse([IO.File]::ReadAllText($build.Manifest))
        try {
            $files=[Collections.Generic.SortedDictionary[string,object]]::new([StringComparer]::Ordinal)
            foreach ($file in $document.RootElement.GetProperty('files').EnumerateArray()) {
                $files.Add($file.GetProperty('relativePath').GetString(),[pscustomobject]@{
                    ByteLength=$file.GetProperty('byteLength').GetInt64();Sha256=$file.GetProperty('sha256').GetString()
                })
            }
            $manifests.Add($files)
        } finally {$document.Dispose()}
    }
    if (-not [Linq.Enumerable]::SequenceEqual[string]($manifests[0].Keys,$manifests[1].Keys,[StringComparer]::Ordinal)) {throw 'Bundle payload membership differs.'}
    $qualifiedPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $variances=[Collections.Generic.List[object]]::new()
    foreach ($architecture in @('arm64','x64','x86')) {
        foreach ($fileName in @('JpgSpinner.winmd','AppxBlockMap.xml')) {
            $relativePath="$architecture/$fileName"
            if (-not $manifests[0].ContainsKey($relativePath)) {throw "Bundle payload lacks a required comparison input: $relativePath"}
            [void]$qualifiedPaths.Add($relativePath)
        }
        $firstPayload=Join-Path $builds[0].Inspection "payloads/$architecture"
        $secondPayload=Join-Path $builds[1].Inspection "payloads/$architecture"
        # The predicate independently qualifies actual WinMD bytes, then only
        # their directly consequent block fields and permutation of complete
        # File siblings. It cannot waive EXE, PRI, dependency metadata, envelope
        # gaps, per-file literal bytes, ordered blocks or header differences.
        $blockMap=& (Join-Path $PSScriptRoot 'Test-JpgSpinnerBlockMapVariance.ps1') `
            -FirstBlockMapPath (Join-Path $firstPayload AppxBlockMap.xml) -SecondBlockMapPath (Join-Path $secondPayload AppxBlockMap.xml) `
            -FirstWinmdPath (Join-Path $firstPayload JpgSpinner.winmd) -SecondWinmdPath (Join-Path $secondPayload JpgSpinner.winmd)
        if (-not $blockMap.ByteIdentical -or -not $blockMap.WinmdIdentity.ByteIdentical) {
            $variances.Add([pscustomobject]@{
                Architecture=$architecture;WinmdIdentity=$blockMap.WinmdIdentity;ChangedBlocks=$blockMap.ChangedBlocks
                BlockMapComparison=[pscustomobject]@{
                    Policy=$blockMap.Policy;FileOrderVarianceObserved=$blockMap.FileOrderVarianceObserved
                    FirstFileOrder=$blockMap.FirstFileOrder;SecondFileOrder=$blockMap.SecondFileOrder
                }
            })
        }
    }
    foreach ($entry in $manifests[0].GetEnumerator()) {
        $other=$manifests[1][$entry.Key]
        if (-not $qualifiedPaths.Contains($entry.Key) -and
            ($entry.Value.ByteLength -ne $other.ByteLength -or $entry.Value.Sha256 -cne $other.Sha256)) {
            throw "Bundle payload differs outside the closed WinMD comparison: $($entry.Key)"
        }
    }
    $record=[pscustomobject]@{
        schemaVersion=1;policy='jpg-spinner-package-payload-comparison-v2'
        comparisonSatisfied=$true
        payloadsByteIdentical=($builds[0].PayloadManifestSha256 -ceq $builds[1].PayloadManifestSha256)
        containersByteIdentical=($builds[0].BundleSha256 -ceq $builds[1].BundleSha256)
        # Evidence remains relocatable when its owning candidate is promoted.
        # These paths are relative to this report, never obsolete staging roots.
        builds=@(foreach ($build in $builds) {[pscustomobject]@{
            inspectionRelativePath=[IO.Path]::GetFileName($build.Inspection)
            payloadManifestRelativePath=[IO.Path]::GetFileName($build.Manifest)
            bundleSha256=$build.BundleSha256;payloadManifestSha256=$build.PayloadManifestSha256
        }})
        observedVariances=$variances.ToArray();releaseApproval='pending'
    }
    [IO.File]::WriteAllText((Join-Path $working 'package-payload-comparison.json'),($record | ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
    $record
} finally {foreach($handle in $handles){$handle.Dispose()}}
