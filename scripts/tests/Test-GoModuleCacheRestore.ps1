<# Exercise native cache preparation using an authenticated offline proxy.
   This reaches the public restore boundary with a real trailing-space fixture;
   no network, synthetic checksum, vendor edits or application build is used. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    # Explicit alternate source is only for isolated regression replay.
    [string]$RestoreScriptPath=(Join-Path (Split-Path -Parent $PSScriptRoot) 'Restore-GoModuleCache.ps1')
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('cache-restore-'+[Guid]::NewGuid().ToString('N'))
$source=Join-Path $fixture source
[void][IO.Directory]::CreateDirectory($source)
[IO.File]::WriteAllText((Join-Path $source go.mod),
    "module jpg-spinner.invalid/cache-restore-test`n`ngo 1.27.0`n`nrequire github.com/vbatts/go-mtree v0.7.0`n")
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'eng/artifact-sbom/go.sum') -Destination (Join-Path $source go.sum)
# Promoting this transitive module to the fixture's root requirement needs its
# own go.mod checksum. This is the native authenticated download's observed
# GoModSum; the production lock is neither incomplete nor modified here.
[IO.File]::AppendAllText((Join-Path $source go.sum),
    "github.com/vbatts/go-mtree v0.7.0/go.mod h1:EjdpFC+LZy1TXbRGNa1MKKgjQ+7ew3foMFJK8o4/TdY=`n")
$hashes=@{}
foreach ($name in @('go.mod','go.sum')) {$hashes[$name]=(Get-FileHash -LiteralPath (Join-Path $source $name)).Hash}
$environmentBefore=@{}
foreach ($name in @('GOMODCACHE','GOCACHE','GOPROXY','GOSUMDB','GOFLAGS')) {
    $environmentBefore[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$parameters=@{
    GoExecutablePath=$GoExecutablePath;ModuleDirectory=$source
    ModuleCacheDirectory=(Join-Path $fixture modules);BuildCacheDirectory=(Join-Path $fixture compiled)
    WorkingDirectory=(Join-Path $fixture preparation)
    Proxy=([Uri]::new((Join-Path $ModuleCacheDirectory 'cache/download')+[IO.Path]::DirectorySeparatorChar)).AbsoluteUri
}
$result=& $RestoreScriptPath @parameters
$literalFile='\\?\'+(Join-Path $parameters.ModuleCacheDirectory 'github.com/vbatts/go-mtree@v0.7.0/testdata/dirwithbrokenlink/ file with spaces ')
if (-not [IO.File]::Exists($literalFile)) {throw 'Native preparation lost the archive filename.'}
if ($result.ModuleCacheDirectory -cne $parameters.ModuleCacheDirectory -or
    -not [IO.File]::ReadAllText((Join-Path $parameters.WorkingDirectory 'integrity.stdout.log')).Contains('all modules verified',[StringComparison]::Ordinal)) {
    throw 'Native preparation returned before verified cache completion.'
}
foreach ($name in $hashes.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $source $name)).Hash -cne $hashes[$name]) {throw 'Native preparation modified its source lock.'}
}
foreach ($name in $environmentBefore.Keys) {
    if ([Environment]::GetEnvironmentVariable($name,'Process') -cne $environmentBefore[$name]) {throw 'Native preparation changed the host environment.'}
}
# Existing cache ownership is not an invitation to overwrite or auto-repair.
$message=$null
try {& $RestoreScriptPath @parameters | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('fresh and absent',[StringComparison]::Ordinal)) {throw 'Existing cache or work was not refused.'}
Write-Output 'PASS: native authenticated restore preserves literal filenames, locks and host settings; existing outputs are refused.'
