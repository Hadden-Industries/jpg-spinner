<# Snapshot the installed closure of the app's JPEG and metadata roots through
   native vcpkg export, then retain unmodified target-triplet SPDX evidence and
   a required-component contract. This is build-component coverage, not proof
   that every dependency contributes code to the linked application. Host tools
   stay in the raw export; test-only Catch2 is not an application root.
   Caller supplies a qualified standalone tool root and quiescent installed
   tree, plus new ordinary external working/evidence directories. No dependency
   resolution, installation, signing or automatic cleanup occurs here. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x86','x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$InstalledRoot,
    [Parameter(Mandatory)][string]$VcpkgRoot,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent $PSScriptRoot
$installed=[IO.Path]::GetFullPath($InstalledRoot)
$toolRoot=[IO.Path]::GetFullPath($VcpkgRoot)
$evidence=[IO.Path]::GetFullPath($EvidenceDirectory)
$working=[IO.Path]::GetFullPath($WorkingDirectory)
$triplet="$Architecture-windows-static-md"
if (-not [IO.Directory]::Exists($installed) -or -not [IO.Directory]::Exists($toolRoot)) {throw 'Installed and standalone tool roots must exist.'}
if ((Test-Path -LiteralPath $evidence) -or (Test-Path -LiteralPath $working)) {throw 'Component evidence and working directories must be fresh and absent.'}
foreach ($path in @($installed,$toolRoot,$evidence,$working)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Component export refuses reparse-point roots or output ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
foreach ($inputRoot in @($installed,$toolRoot)) {
    foreach ($destination in @($working,$evidence)) {
        $relative=[IO.Path]::GetRelativePath($inputRoot,$destination)
        if (-not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)) {
            throw 'Component outputs must be outside installed and tool roots.'
        }
    }
}
foreach ($directory in @($working,$evidence)) {
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($directory))) {throw 'Component output parents must exist.'}
}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$pins=ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content (Join-Path $repositoryRoot 'eng/sbom-tools.json') -Raw) -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
$executable=Join-Path $toolRoot 'vcpkg.exe'
if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins.vcpkgExporter.executableSha256) {
    throw 'Native vcpkg exporter identity differs from the reviewed pin.'
}
[void][IO.Directory]::CreateDirectory($working)
function Invoke-InstalledVcpkg {
    # Use the explicit isolated distribution, not PATH or the build's VS bundle.
    # vcpkg owns installed-state locks, graph traversal and native export syntax.
    param([string[]]$Arguments,[string]$LogName)
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$executable
    $start.WorkingDirectory=$repositoryRoot
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    foreach ($argument in $Arguments + @('--classic','--disable-metrics',"--vcpkg-root=$toolRoot","--x-install-root=$installed")) {$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new()
    $process.StartInfo=$start
    try {
        if (-not $process.Start()) {throw 'Native vcpkg did not start.'}
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(180000)) {
            $process.Kill($true)
            if (-not $process.WaitForExit(5000)) {throw 'Owned vcpkg termination remains unresolved.'}
            throw 'Native vcpkg exceeded its deadline.'
        }
        $text=$stdout.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $working "$LogName.stdout.log"),$text)
        [IO.File]::WriteAllText((Join-Path $working "$LogName.stderr.log"),$stderr.GetAwaiter().GetResult())
        if ($process.ExitCode -ne 0) {throw "Native vcpkg refused $LogName (exit $($process.ExitCode))."}
        return $text
    } finally {$process.Dispose()}
}
$installedJson=Invoke-InstalledVcpkg -Arguments @('list','--x-json') -LogName installed-list
$installedPackages=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $installedJson -RequiredRootValueKind Object -SourceDescription 'Native installed package list' -AsHashtable
$roots=@('exiv2','libjpeg-turbo')
$exportRoot=Join-Path $working 'native-export'
[void](Invoke-InstalledVcpkg -Arguments (@('export','--raw',"--output-dir=$working",'--output=native-export') + @($roots | ForEach-Object {"${_}:$triplet"})) -LogName installed-export)
$share=Join-Path $exportRoot "installed/$triplet/share"
$required=[Collections.Generic.List[object]]::new()
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
[void][IO.Directory]::CreateDirectory($evidence)
# Native export selects the installed transitive closure. Enumerate its native
# per-port SPDX documents, not the original direct roots or a hand-built graph.
# Extra share directories (jpeg, XmpSdk, documentation) are not extra ports.
# https://learn.microsoft.com/vcpkg/commands/export
foreach ($directory in Get-ChildItem -LiteralPath $share -Directory) {
    if (($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Exported metadata directory is a reparse point.'}
    $source=Join-Path $directory.FullName 'vcpkg.spdx.json'
    if (-not [IO.File]::Exists($source)) {continue}
    if (([IO.File]::GetAttributes($source) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Exported metadata is a reparse point.'}
    $name=$directory.Name
    $key="${name}:$triplet"
    if (-not $installedPackages.Contains($key)) {throw 'Exported target component is absent from the native installed list.'}
    $identity=$installedPackages[$key]
    if ($identity.package_name -cne $name -or $identity.triplet -cne $triplet -or
        $identity.version -isnot [string] -or [string]::IsNullOrEmpty($identity.version) -or
        $identity.port_version -isnot [long] -or $identity.port_version -lt 0) {throw 'Native installed component identity has an unsupported shape.'}
    $original=Join-Path $installed "$triplet/share/$name/vcpkg.spdx.json"
    if ((Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash) {
        throw 'Exported metadata differs from the installed build evidence.'
    }
    $document=ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $source -Raw) -RequiredRootValueKind Object -SourceDescription 'Native vcpkg SPDX evidence' -AsHashtable
    $port=@($document.packages | Where-Object SPDXID -CEQ 'SPDXRef-port')
    $version=$identity.version
    if ($identity.port_version -ne 0) {$version+="#$($identity.port_version)"}
    if ($document.spdxVersion -cne 'SPDX-2.3' -or $port.Count -ne 1 -or $port[0].name -cne $name -or $port[0].versionInfo -cne $version) {
        throw 'Native SPDX port identity differs from the independent installed list.'
    }
    # Preserve the vendor's opaque package URL. Its qualifier spelling/order is
    # native data, not a second URL grammar maintained by the release scripts.
    $packageUrls=@($port[0].externalRefs | Where-Object { $_.referenceCategory -ceq 'PACKAGE-MANAGER' -and $_.referenceType -ceq 'purl' })
    if ($packageUrls.Count -ne 1 -or $packageUrls[0].referenceLocator -isnot [string] -or [string]::IsNullOrEmpty($packageUrls[0].referenceLocator) -or -not $seen.Add($name)) {
        throw 'Native SPDX component identity is absent or ambiguous.'
    }
    $destination=Join-Path $evidence "installed/$triplet/share/$name/vcpkg.spdx.json"
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
    [IO.File]::Copy($source,$destination,$false)
    $required.Add(@{name=$name;version=$version;packageUrl=$packageUrls[0].referenceLocator})
}
foreach ($name in $roots) {if (-not $seen.Contains($name)) {throw 'Native export omitted a required application root.'}}
# vcpkg emits one ownership list per exported package. Cross-check target
# cardinality as well as the direct roots, so a missing transitive SPDX document
# cannot silently reduce the required-component contract. This consumes the
# native layout's triplet suffix; it neither parses list contents nor walks a
# dependency graph independently of vcpkg.
$ownershipLists=@(Get-ChildItem -LiteralPath (Join-Path $exportRoot 'installed/vcpkg/info') -File -Filter '*.list' |
    Where-Object {$_.Name.EndsWith("_$triplet.list",[StringComparison]::Ordinal)})
if ($ownershipLists.Count -ne $required.Count) {throw 'Native export component metadata does not cover every target ownership list.'}
$contract=Join-Path $evidence 'required-components.json'
@{requiredPackages=@($required)} | ConvertTo-Json -Depth 8 | Out-File -LiteralPath $contract -Encoding utf8NoBOM -NoClobber
[pscustomobject]@{ComponentEvidenceRoot=$evidence;ComponentContractPath=$contract;NativeExportRoot=$exportRoot}
