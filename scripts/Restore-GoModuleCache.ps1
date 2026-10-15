<# Prepare a fresh private Go cache from unchanged lock snapshots. Native Go
   owns authentication, extraction and integrity; this orchestration does not
   resolve versions, repair existing caches, compile or install system tools.
   Caller owns quiescent ordinary inputs and disjoint fresh external outputs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleDirectory,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$BuildCacheDirectory,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [string]$Proxy='https://proxy.golang.org'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$moduleRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ModuleDirectory)
$moduleCache=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ModuleCacheDirectory)
$buildCache=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildCacheDirectory)
$working=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
$roots=@($moduleRoot,$moduleCache,$buildCache,$working)
foreach ($path in $roots) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Native cache preparation refuses redirected roots or ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
    foreach ($other in $roots) {
        if ($path -ceq $other) {continue}
        $relative=[IO.Path]::GetRelativePath($path,$other)
        if (-not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
            -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)) {
            throw 'Native cache preparation requires disjoint input, cache and work roots.'
        }
    }
}
if (@($roots | Select-Object -Unique).Count -ne 4) {throw 'Native cache preparation requires distinct roots.'}
foreach ($destination in @($moduleCache,$buildCache,$working)) {
    if (Test-Path -LiteralPath $destination) {throw 'Native cache and working directories must be fresh and absent.'}
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {throw 'Native cache output parents must exist.'}
}
$hashes=@{}
foreach ($name in @('go.mod','go.sum')) {
    $path=Join-Path $moduleRoot $name
    if (-not [IO.File]::Exists($path) -or ([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Native cache preparation requires ordinary locked module inputs.'
    }
    $hashes[$name]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}
[void][IO.Directory]::CreateDirectory($working)
$snapshot=Join-Path $working locked-inputs
[void][IO.Directory]::CreateDirectory($snapshot)
foreach ($name in $hashes.Keys) {Copy-Item -LiteralPath (Join-Path $moduleRoot $name) -Destination (Join-Path $snapshot $name)}
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1') -Force
# The namespace must be literal during extraction, not only verification.
# Do not use it for later compilation: Go embed globs interpret '?' specially.
# https://learn.microsoft.com/windows/win32/fileio/naming-a-file
$extractionCache=$moduleCache
if ($IsWindows -and -not $moduleCache.StartsWith('\\?\',[StringComparison]::Ordinal)) {
    $extractionCache=if($moduleCache.StartsWith('\\',[StringComparison]::Ordinal)){'\\?\UNC\'+$moduleCache.Substring(2)}else{'\\?\'+$moduleCache}
}
$environmentValues=@{
    GOMODCACHE=$extractionCache;GOCACHE=$buildCache;GOMAXPROCS='2';GOTOOLCHAIN='local'
    GOWORK='off';GOENV='off';GOFLAGS='';GOPROXY=$Proxy;GOSUMDB='sum.golang.org'
    GOPRIVATE='';GONOSUMDB='';GONOPROXY=''
}
$toolContext=@{Executable=$GoExecutablePath;WorkingDirectory=$snapshot;LogDirectory=$working;EnvironmentValues=$environmentValues}
try {
    # With this modern go directive, no pattern means explicit requirements,
    # not the broader 'all' graph. Native Go and the copied locks own selection.
    [void](Invoke-ArtifactToolProcess @toolContext -Arguments @('mod','download','-json') -LogName download)
    [void](Invoke-ArtifactToolProcess @toolContext -Arguments @('mod','verify') -LogName integrity -EnvironmentOverrides @{GOPROXY='off'})
} finally {
    foreach ($name in $hashes.Keys) {
        foreach ($root in @($moduleRoot,$snapshot)) {
            if ((Get-FileHash -LiteralPath (Join-Path $root $name) -Algorithm SHA256).Hash -cne $hashes[$name]) {
                throw 'Native preparation changed a source or staged dependency lock; candidate cache is unqualified.'
            }
        }
    }
}
[pscustomobject]@{ModuleCacheDirectory=$moduleCache;BuildCacheDirectory=$buildCache;WorkingDirectory=$working}
