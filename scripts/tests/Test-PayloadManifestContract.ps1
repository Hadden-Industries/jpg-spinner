<# Literal paths/lengths/SHA-256 values independently characterize a fixed
   payload. Identical bytes under different roots must yield identical output. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer = Join-Path $repositoryRoot 'scripts/New-PayloadManifest.ps1'
$fixture = Join-Path $PSScriptRoot 'PayloadManifestFixture'
$output = Join-Path $OwnedOutputRoot 'payload-contract.json'
if (Test-Path -LiteralPath $output) { throw 'Test output must be fresh.' }
& $producer -PayloadRoot $fixture -OutputPath $output
Import-Module (Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1') -Force
$manifest = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $output -Raw) `
    -SourceDescription 'Actual payload manifest' -RequiredRootValueKind Object -AsHashtable
if ($manifest.schemaVersion -ne 1 -or $manifest.files.Count -ne 2) { throw 'Payload manifest shape differs from the accepted contract.' }
# SHA-256 of ASCII hello plus LF is an independently fixed vector.
$expected = @(
    @{relativePath='Z.txt';byteLength=6;sha256='5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03'},
    @{relativePath='nested/a.txt';byteLength=6;sha256='5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03'}
)
for ($index = 0; $index -lt $expected.Count; ++$index) {
    foreach ($field in @('relativePath','byteLength','sha256')) {
        if ($manifest.files[$index][$field] -cne $expected[$index][$field]) { throw "Payload contract differs at $index / $field." }
    }
}
$bytes = [IO.File]::ReadAllBytes($output)
if ($bytes[0] -eq 0xef -or $bytes[-1] -ne 10 -or $bytes -contains 13) { throw 'Manifest must be BOM-free UTF-8 with LF terminator.' }
Write-Output 'PASS: fixed payload paths, ordinal ordering, byte lengths and independent SHA-256 vectors.'
