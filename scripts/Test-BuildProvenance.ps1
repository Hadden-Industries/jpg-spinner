<# Validate observed build inputs with native strict JSON and JSON Schema.
   This validates record shape and current pins, not execution or release proof. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Path)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$json = Get-Content -LiteralPath $Path -Raw
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
# The platform strict-JSON reader rejects duplicate members and non-object
# roots before its own PowerShell conversion. Do not use the converted record
# for any decision until the original JSON also passes native schema validation.
$record = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -SourceDescription 'Build provenance' `
    -RequiredRootValueKind Object -AsHashtable
$schemaErrors = @()
# Validate the original bytes against the native schema engine, retaining its
# complete diagnostics. Local $refs only: no remote schema fetch is required.
$valid = Test-Json -Json $json -SchemaFile (Join-Path $root 'eng/build-provenance.schema.json') `
    -ErrorAction Continue -ErrorVariable schemaErrors 2>$null
if (-not $valid) { throw "Build provenance schema refused: $($schemaErrors -join '; ')" }
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (
    Get-Content -LiteralPath (Join-Path $root 'eng/github-actions-lock.json') -Raw
) -SourceDescription 'Approved action pins' -RequiredRootValueKind Object -AsHashtable
# The pin file owns action membership as well as values. Keep the schema's
# structural hash contract independent of that changing inventory; reject both
# missing and additional actions rather than maintaining a second name enum.
if ($record.actionCommits.Count -ne $pins.actions.Count) {throw 'Build provenance action membership differs from the reviewed pins.'}
foreach ($action in $pins.actions.Keys) {
    if (-not $record.actionCommits.Contains($action) -or $record.actionCommits[$action] -cne $pins.actions[$action].commit) { throw 'Build provenance differs from current reviewed action pins.' }
}
Write-Output 'PASS: strict build provenance schema, unique members, normalized paths and current action pins.'
