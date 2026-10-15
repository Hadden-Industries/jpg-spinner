<# Retain the real large-document resource reproducer. Both real validation and
   semantic type refusals must finish inside the production child bounds. No
   document/schema repair, validator stub or assertion suppression is allowed. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$QualifiedManifestPath,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SchemaValidatorToolsDirectory,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer=Join-Path $repositoryRoot 'scripts/Test-SpdxJsonSchema.ps1'
$fixture=Join-Path $OwnedOutputRoot ('schema-controls-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$before=(Get-FileHash -LiteralPath $QualifiedManifestPath).Hash
$arguments=@{SpdxSchemaPath=$SpdxSchemaPath;SchemaValidatorToolsDirectory=$SchemaValidatorToolsDirectory}
& $producer -ManifestPath $QualifiedManifestPath @arguments
foreach ($case in @(
    @{Name='wrong-context';Mutate={param($doc) $doc['@context']='https://spdx.org/rdf/3.0.0/spdx-context.jsonld'}},
    @{Name='unknown-class';Mutate={param($doc) $doc['@graph'][0].type='UnqualifiedClass'}},
    @{Name='wrong-json-type';Mutate={param($doc) $doc['@graph'][0].creationInfo=42}}
)) {
    $candidate=Get-Content -LiteralPath $QualifiedManifestPath -Raw | ConvertFrom-Json -AsHashtable
    & $case.Mutate $candidate
    $path=Join-Path $fixture "$($case.Name).spdx.json"
    $candidate | ConvertTo-Json -Depth 100 | Out-File -LiteralPath $path -Encoding utf8NoBOM -NoClobber
    $message=$null
    try {& $producer -ManifestPath $path @arguments | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.StartsWith('Official SPDX 3.0.1 schema rejected native output.',[StringComparison]::Ordinal)) {
        throw "Official schema did not refuse '$($case.Name)' through native validation: $message"
    }
}
$wrongTools=Join-Path $fixture 'wrong-tools'
[void][IO.Directory]::CreateDirectory($wrongTools)
[IO.File]::WriteAllText((Join-Path $wrongTools 'package-lock.json'),'{}')
[IO.File]::WriteAllText((Join-Path $wrongTools 'package.json'),'{}')
$message=$null
try {& $producer -ManifestPath $QualifiedManifestPath -SpdxSchemaPath $SpdxSchemaPath -SchemaValidatorToolsDirectory $wrongTools | Out-Null}
catch {$message=$_.Exception.Message}
if ($message -cne 'SPDX schema validator tool definitions differ from the reviewed lock.') {throw "Wrong native tool lock was not refused before execution: $message"}
if ((Get-FileHash -LiteralPath $QualifiedManifestPath).Hash -cne $before) {throw 'Native schema validation changed the original input.'}
Write-Output 'PASS: bounded real-document validation, wrong context/class/JSON type and tool-lock refusals, preserving the original native document.'
