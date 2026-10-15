<# Qualify Windows cache preparation using native Go and an authentic cached
   upstream module containing a trailing-space filename. Ordinary extraction
   must expose the retained defect; literal extraction and verification must
   preserve the archive. This is not application or whole-SDK qualification.
   Caller owns a short external output root; no acquisition or repair occurs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not $IsWindows) {throw 'This qualification requires native Windows filename semantics.'}
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$working=Join-Path $OwnedOutputRoot ('cache-names-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($working)
$inputs=Join-Path $working inputs
[void][IO.Directory]::CreateDirectory($inputs)
# A fixed regression input, not a second production dependency selection.
# The authentic archive and existing application-tool lock own its bytes.
$module='github.com/vbatts/go-mtree'
$version='v0.7.0'
$relativeFixture='testdata/dirwithbrokenlink/ file with spaces '
[IO.File]::WriteAllText((Join-Path $inputs go.mod),
    "module jpg-spinner.invalid/cache-namespace-test`n`ngo 1.27.0`n`nrequire $module $version`n")
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'eng/artifact-sbom/go.sum') -Destination (Join-Path $inputs go.sum)
$archives=Join-Path $ModuleCacheDirectory "cache/download/$module/@v"
$archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $archives "$version.zip"))
try {
    if ($null -eq $archive.GetEntry("$module@$version/$relativeFixture")) {
        throw 'Upstream fixture lacks the literal trailing-space filename.'
    }
} finally {$archive.Dispose()}
# Reuse the actual bounded native-process boundary, including diagnostic and
# termination behavior, rather than introduce a second test process runner.
$tokens=$null;$parseErrors=$null
$syntax=[Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $repositoryRoot 'scripts/New-ArtifactSbom.ps1'),[ref]$tokens,[ref]$parseErrors)
$helpers=@($syntax.FindAll({param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -ceq 'Invoke-ArtifactToolProcess'
},$true))
if ($parseErrors.Count -ne 0 -or $helpers.Count -ne 1) {throw 'Native artifact process boundary is unavailable.'}
$invokeProcess=$helpers[0].Body.GetScriptBlock()
foreach ($namespace in @('ordinary','literal')) {
    $cache=Join-Path $working $namespace
    $destination=Join-Path $cache "cache/download/$module/@v"
    [void][IO.Directory]::CreateDirectory($destination)
    foreach ($extension in @('info','mod','zip','ziphash')) {
        Copy-Item -LiteralPath (Join-Path $archives "$version.$extension") -Destination $destination
    }
    $literalCache='\\?\'+$cache
    $literalFixture=Join-Path $literalCache "$module@$version/$relativeFixture"
    # Go may automatically choose the extended namespace for long paths.
    # Keep the ordinary negative control short enough to exercise its cause.
    if ($literalFixture.Length -ge 260) {throw 'Choose a shorter owned output root for the ordinary-path negative control.'}
    $settings=@{
        GOMODCACHE=$(if($namespace -ceq 'literal'){$literalCache}else{$cache})
        GOCACHE=(Join-Path $working build-cache)
        GOMAXPROCS='2';GOTOOLCHAIN='local';GOWORK='off';GOENV='off';GOFLAGS=''
        GOPROXY='off';GOSUMDB='sum.golang.org'
    }
    & $invokeProcess -Executable $GoExecutablePath -Arguments @('-C',$inputs,'mod','download','-json',"$module@$version") `
        -LogName "$namespace-extraction" -TimeoutSeconds 20 | Out-Null
    $failure=$null
    try {
        & $invokeProcess -Executable $GoExecutablePath -Arguments @('-C',$inputs,'mod','verify') `
            -EnvironmentOverrides @{GOMODCACHE=$literalCache} -LogName "$namespace-integrity" -TimeoutSeconds 20 | Out-Null
    } catch {$failure=$_.Exception.Message}
    $diagnostic=[IO.File]::ReadAllText((Join-Path $working "$namespace-integrity.stderr.log"))
    if ($namespace -ceq 'ordinary') {
        if ($null -eq $failure -or -not $diagnostic.Contains('dir has been modified',[StringComparison]::Ordinal) -or
            [IO.File]::Exists($literalFixture)) {throw 'Ordinary extraction did not reproduce the normalized-filename integrity defect.'}
    } elseif ($null -ne $failure -or -not [IO.File]::Exists($literalFixture)) {
        throw 'Literal native extraction failed to preserve and verify the upstream fixture.'
    }
}
Write-Output 'PASS: ordinary extraction reproduces the defect; literal native extraction preserves the filename and passes integrity.'
