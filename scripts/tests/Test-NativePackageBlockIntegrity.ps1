<# Characterize the actual owning SDK, not a replacement MSIX validator.
   Native ZIP/XML writers create one intentionally invalid owned copy with a
   well-formed but false WinMD block hash. Original package bytes stay intact.
   Full sequential extraction must reject the false hash through the native
   reader; semantic XML validity or a valid ZIP CRC alone is insufficient.
   https://learn.microsoft.com/windows/win32/api/appxpackaging/nf-appxpackaging-iappxfile-getstream #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('native-block-integrity-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$before=(Get-FileHash -LiteralPath $PackagePath).Hash
$toolchain=& (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
Push-Location -LiteralPath $root
try {
    $makeAppx=& $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') -nologo -noAutoResponse -p:Platform=x64 -p:GenerateAppxPackageOnBuild=true '-getProperty:MakeAppxExeFullPath'
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($makeAppx)) {throw 'Native block-integrity fixture cannot resolve the owning SDK.'}
    & $makeAppx unpack /p $PackagePath /d (Join-Path $fixture positive) /no *> (Join-Path $fixture positive.log)
    if ($LASTEXITCODE -ne 0) {throw 'Native positive package did not pass extraction before the negative control.'}
    $invalid=Join-Path $fixture false-block-hash.msix
    Copy-Item -LiteralPath $PackagePath -Destination $invalid
    $archive=[IO.Compression.ZipFile]::Open($invalid,[IO.Compression.ZipArchiveMode]::Update)
    try {
        $entry=$archive.GetEntry('AppxBlockMap.xml')
        if ($null -eq $entry) {throw 'Native package fixture lacks its block map.'}
        $stream=$entry.Open()
        try {$map=[Xml.XmlDocument]::new();$map.Load($stream)} finally {$stream.Dispose()}
        $namespaces=[Xml.XmlNamespaceManager]::new($map.NameTable)
        $namespaces.AddNamespace('b','http://schemas.microsoft.com/appx/2010/blockmap')
        $blocks=$map.SelectNodes('/b:BlockMap/b:File[@Name="JpgSpinner.winmd"]/b:Block',$namespaces)
        if ($blocks.Count -ne 1) {throw 'This minimized block-integrity fixture requires one native WinMD block.'}
        $falseHash=[Convert]::ToBase64String([byte[]]::new(32))
        if ($blocks[0].GetAttribute('Hash') -ceq $falseHash) {throw 'Negative control must change a real native block hash.'}
        $blocks[0].SetAttribute('Hash',$falseHash)
        $entry.Delete()
        $replacement=$archive.CreateEntry('AppxBlockMap.xml',[IO.Compression.CompressionLevel]::NoCompression)
        $stream=$replacement.Open()
        try {$map.Save($stream)} finally {$stream.Dispose()}
    } finally {$archive.Dispose()}
    & $makeAppx unpack /p $invalid /d (Join-Path $fixture negative) /no *> (Join-Path $fixture negative.log)
    if ($LASTEXITCODE -eq 0) {throw 'Native extraction accepted a false WinMD block hash.'}
    $diagnostic=[IO.File]::ReadAllText((Join-Path $fixture negative.log))
    if (-not $diagnostic.Contains('0x80080207',[StringComparison]::OrdinalIgnoreCase)) {throw 'Native refusal did not identify the intended APPX_E_BLOCK_HASH_INVALID boundary.'}
} finally {Pop-Location}
if ((Get-FileHash -LiteralPath $PackagePath).Hash -cne $before) {throw 'Native block-integrity qualification changed the original package.'}
Write-Output "PASS: original native package extracts; well-formed false block hash refuses with APPX_E_BLOCK_HASH_INVALID. Evidence: $fixture"
