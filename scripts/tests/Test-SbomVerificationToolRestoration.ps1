<# Exercise real pinned archives, platform extraction and npm's locked restore.
   The supplied downloads are a frozen acquisition snapshot, not mock tools.
   This is provisioning qualification, not application/SBOM/release acceptance. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DownloadDirectory,
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [Parameter(Mandatory)][string]$NodeExecutablePath,
    [Parameter(Mandatory)][string]$NpmCliPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('sbom-tools-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$before=@{}
foreach($file in Get-ChildItem -LiteralPath $DownloadDirectory -File){$before[$file.Name]=(Get-FileHash -LiteralPath $file.FullName).Hash}
$output=Join-Path $fixture restored
$parameters=@{DownloadDirectory=$DownloadDirectory;OutputDirectory=$output;NodeExecutablePath=$NodeExecutablePath;NpmCliPath=$NpmCliPath}
$tools=& (Join-Path $root scripts/Restore-SbomVerificationTools.ps1) @parameters
foreach($path in @((Join-Path $tools.VcpkgRoot vcpkg.exe),$tools.SpdxSchemaPath,$tools.SpdxToolsJarPath,(Join-Path $tools.SchemaValidatorToolsDirectory node_modules/ajv/package.json))){
    if(-not [IO.File]::Exists($path)){throw 'A restored native verification tool is absent.'}
}
$refusal=$null
try {& (Join-Path $root scripts/Restore-SbomVerificationTools.ps1) @parameters | Out-Null} catch {$refusal=$_.Exception.Message}
if($refusal -cne 'SBOM tool restoration requires a fresh external output directory.'){throw "Existing tools were not refused: $refusal"}
# Corrupt a copied executable only. The complete byte-identity check must refuse
# before any extraction/npm execution, not rely on a later launch failure.
$corrupt=Join-Path $fixture corrupt
[void][IO.Directory]::CreateDirectory($corrupt)
foreach($file in Get-ChildItem -LiteralPath $DownloadDirectory -File){[IO.File]::Copy($file.FullName,(Join-Path $corrupt $file.Name),$false)}
$corruptExecutable=Join-Path $corrupt vcpkg.exe
$stream=[IO.File]::Open($corruptExecutable,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {$original=$stream.ReadByte();$stream.Position=0;$stream.WriteByte($original -bxor 1)} finally {$stream.Dispose()}
$refusedOutput=Join-Path $fixture refused
$parameters.DownloadDirectory=$corrupt;$parameters.OutputDirectory=$refusedOutput
$refusal=$null
try {& (Join-Path $root scripts/Restore-SbomVerificationTools.ps1) @parameters | Out-Null} catch {$refusal=$_.Exception.Message}
if($refusal -cne 'SBOM verification download differs from its reviewed digest.' -or (Test-Path -LiteralPath $refusedOutput)){throw 'Changed tool bytes were not refused before effects.'}
foreach($entry in $before.GetEnumerator()){
    if((Get-FileHash -LiteralPath (Join-Path $DownloadDirectory $entry.Key)).Hash -cne $entry.Value){throw 'Tool restoration changed an original download.'}
}
Write-Output 'PASS: native pinned-tool restoration, locked npm input preservation, early reused-output/tampered-tool refusal and unchanged original downloads.'
Write-Output $tools
