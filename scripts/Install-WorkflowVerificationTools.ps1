<# Acquire only the reviewed native YAML/workflow validators into a tool cache.
   Does not execute workflow steps, modify installed toolchains or build code. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -ne
    [Runtime.InteropServices.Architecture]::X64) { throw 'Workflow verification tools require Windows x64 PowerShell.' }
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (
    Get-Content -LiteralPath (Join-Path $repositoryRoot 'eng/workflow-verification-tools.json') -Raw
) -SourceDescription 'Workflow verification tool pins' -RequiredRootValueKind Object -AsHashtable
if ($pins.schemaVersion -ne 1) { throw 'Unsupported workflow verification tool-pin contract.' }
$cache = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ToolsDirectory)
[void][IO.Directory]::CreateDirectory($cache)
$resolvedPaths = @{}
foreach ($toolName in @('yamlDotNet','actionlint')) {
    $pin = $pins[$toolName]
    $uri = [Uri]::new($pin.url,[UriKind]::Absolute)
    if ($uri.Scheme -cne 'https' -or $pin.archiveSha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Tool URL/digest pin is invalid.' }
    $archive = Join-Path $cache ([IO.Path]::GetFileName($uri.AbsolutePath))
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
        Invoke-WebRequest -Uri $uri -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.archiveSha256) {
        # Retain the mismatched bytes as counterevidence; never execute them or
        # silently replace them with a different version or a moving URL.
        throw "Pinned $toolName archive digest differs; acquisition refused."
    }
    $directory = Join-Path $cache ($toolName + '-' + $pin.version)
    if (-not (Test-Path -LiteralPath $directory)) {
        [IO.Compression.ZipFile]::ExtractToDirectory($archive,$directory)
    }
    $relativePath = if ($toolName -ceq 'yamlDotNet') { $pin.assemblyRelativePath } else { 'actionlint.exe' }
    $resolvedPath = Join-Path $directory $relativePath
    if (-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)) { throw "Pinned $toolName payload is absent." }
    # A valid cached ZIP does not authenticate a previously extracted DLL/EXE.
    # Re-read its exact native archive entry and compare hashes before loading;
    # no second ZIP parser or unverified cache fallback is introduced.
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $entry = $zip.GetEntry($relativePath)
        if ($null -eq $entry) { throw "Pinned $toolName archive entry is absent." }
        $entryStream = $entry.Open()
        try { $entryHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($entryStream)) }
        finally { $entryStream.Dispose() }
    }
    finally { $zip.Dispose() }
    if ((Get-FileHash -LiteralPath $resolvedPath -Algorithm SHA256).Hash -cne $entryHash) { throw "Cached $toolName executable bytes differ from the pinned archive." }
    $resolvedPaths[$toolName] = $resolvedPath
}
# NuGet carries an MIT expression, not the complete permission notice beside
# each extracted assembly. Preserve the exact reviewed upstream notice too.
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'eng/licenses/YamlDotNet.LICENSE.txt') `
    -Destination (Join-Path $cache 'yamlDotNet-18.1.0/LICENSE.txt')
[pscustomobject]@{
    yamlAssemblyPath = $resolvedPaths.yamlDotNet
    actionlintExecutablePath = $resolvedPaths.actionlint
}
