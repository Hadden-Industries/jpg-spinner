<# Qualify actual installed-state native export, including dependencies absent
   from the app's two direct roots. Independent expected names are retained here;
   this test never uses a parsed console plan or the producer's traversal. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [Parameter(Mandatory)][string]$VcpkgRoot,
    [Parameter(Mandatory)][ValidateSet('x86','x64','arm64')][string]$Architecture
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$triplet="$Architecture-windows-static-md"
$installed=Join-Path $repositoryRoot "vcpkg_installed/$triplet"
$fixture=Join-Path $OwnedOutputRoot ('vcpkg-evidence-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$result=& (Join-Path $repositoryRoot 'scripts/Export-JpgSpinnerVcpkgEvidence.ps1') -Architecture $Architecture -InstalledRoot $installed -VcpkgRoot $VcpkgRoot -EvidenceDirectory (Join-Path $fixture 'components') -WorkingDirectory (Join-Path $fixture 'working')
$contract=Get-Content -LiteralPath $result.ComponentContractPath -Raw | ConvertFrom-Json -AsHashtable
$names=@($contract.requiredPackages.name | Sort-Object)
$expected=@('adobe-xmp-core','exiv2','expat','inih','libjpeg-turbo','libspng','zlib')
if ($names.Count -ne $expected.Count -or @($names | Where-Object {$_ -cnotin $expected}).Count -ne 0) {throw "Installed dependency coverage is incomplete or includes unrelated ports: $($names -join ', ')"}
$ownershipLists=@(Get-ChildItem -LiteralPath (Join-Path $result.NativeExportRoot 'installed/vcpkg/info') -File -Filter '*.list' |
    Where-Object {$_.Name.EndsWith("_$triplet.list",[StringComparison]::Ordinal)})
if ($ownershipLists.Count -ne 7) {throw 'Native target ownership-list fixture precondition changed.'}
foreach ($name in $expected) {
    $relative="$triplet/share/$name/vcpkg.spdx.json"
    $source=Join-Path $installed $relative
    $copy=Join-Path $result.ComponentEvidenceRoot "installed/$relative"
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash) {throw "Native component evidence changed: $name"}
    $identity=@($contract.requiredPackages | Where-Object name -CEQ $name)
    if ($identity.Count -ne 1 -or -not $identity[0].ContainsKey('packageUrl') -or [string]::IsNullOrEmpty($identity[0].packageUrl)) {throw "Native triplet-qualified identity is missing: $name"}
    $original=Get-Content -LiteralPath $source -Raw | ConvertFrom-Json -AsHashtable
    $port=@($original.packages | Where-Object SPDXID -CEQ 'SPDXRef-port')
    $packageUrl=@($port[0].externalRefs | Where-Object referenceType -CEQ 'purl')
    if ($identity[0].version -cne $port[0].versionInfo -or $identity[0].packageUrl -cne $packageUrl[0].referenceLocator) {throw "Native component version or qualified identity changed: $name"}
}
Write-Output "PASS: $Architecture installed dependency closure, unrelated-port exclusion, exact native metadata hashes and qualified identities."
Write-Output $result
