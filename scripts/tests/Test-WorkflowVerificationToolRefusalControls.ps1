<# Poison exclusively owned copies, not the live validator cache. Both archive
   and extracted-payload drift must refuse before a tool path is returned. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory,[Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$installer = Join-Path $repositoryRoot 'scripts/Install-WorkflowVerificationTools.ps1'
$qualifiedTools = & $installer -ToolsDirectory $ToolsDirectory
$root = Join-Path $OwnedOutputRoot ('workflow-tool-refusal-' + [Guid]::NewGuid().ToString('N'))
foreach ($kind in @('archive','assembly')) {
    $fixture = Join-Path $root $kind
    [void][IO.Directory]::CreateDirectory($fixture)
    $archive = Join-Path $fixture 'yamldotnet.18.1.0.nupkg'
    Copy-Item -LiteralPath (Join-Path $ToolsDirectory 'yamldotnet.18.1.0.nupkg') -Destination $archive
    $target = $archive
    $diagnostic = 'archive digest differs'
    if ($kind -ceq 'assembly') {
        $assemblyDirectory = Join-Path $fixture 'yamlDotNet-18.1.0/lib/net10.0'
        [void][IO.Directory]::CreateDirectory($assemblyDirectory)
        $target = Join-Path $assemblyDirectory 'YamlDotNet.dll'
        Copy-Item -LiteralPath $qualifiedTools.yamlAssemblyPath -Destination $target
        $diagnostic = 'executable bytes differ from the pinned archive'
    }
    $stream = [IO.FileStream]::new($target,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try {
        $before = $stream.ReadByte()
        $stream.Position = 0
        $stream.WriteByte([byte]($before -bxor 1))
        $stream.Flush($true)
    }
    finally { $stream.Dispose() }
    $message = $null
    try { & $installer -ToolsDirectory $fixture }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($diagnostic,[StringComparison]::Ordinal)) {
        throw "Poisoned $kind cache was not refused: $message"
    }
    Write-Output "PASS: native $kind cache drift refused before tool loading."
}
