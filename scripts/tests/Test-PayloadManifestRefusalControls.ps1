<# Refuse an existing result and a self-inventory output without mutating the
   fixed payload. A directory junction is created only in UUID-owned scratch. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer = Join-Path $repositoryRoot 'scripts/New-PayloadManifest.ps1'
$fixtureRoot = Join-Path $OwnedOutputRoot ('payload-refusal-' + [Guid]::NewGuid().ToString('N'))
$payloadRoot = Join-Path $fixtureRoot 'payload'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'PayloadManifestFixture') -Destination $payloadRoot -Recurse
$source = Join-Path $payloadRoot 'Z.txt'
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$existingOutput = Join-Path $fixtureRoot 'existing.json'
Copy-Item -LiteralPath $source -Destination $existingOutput
$existingHash = (Get-FileHash -LiteralPath $existingOutput -Algorithm SHA256).Hash
foreach ($case in @(
    @{Input=$payloadRoot;Output=$existingOutput;Diagnostic='fresh and absent'},
    @{Input=$payloadRoot;Output=(Join-Path $payloadRoot 'self.json');Diagnostic='outside the payload root'}
)) {
    $message = $null
    try { & $producer -PayloadRoot $case.Input -OutputPath $case.Output }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal)) {
        throw "Required payload refusal is absent: $message"
    }
}
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $sourceHash -or
    (Get-FileHash -LiteralPath $existingOutput -Algorithm SHA256).Hash -cne $existingHash -or
    (Test-Path -LiteralPath (Join-Path $payloadRoot 'self.json'))) { throw 'Payload refusal mutated input or output.' }

# NTFS directory junction creation needs no symbolic-link privilege. The native
# attribute is asserted so a fixture setup failure cannot masquerade as refusal.
$junction = Join-Path $fixtureRoot 'linked-payload'
[void](New-Item -ItemType Junction -Path $junction -Target $payloadRoot)
if (((Get-Item -LiteralPath $junction).Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) { throw 'Reparse-point fixture precondition is absent.' }
$linkedOutput = Join-Path $fixtureRoot 'linked.json'
$message = $null
try { & $producer -PayloadRoot $junction -OutputPath $linkedOutput }
catch { $message = $_.Exception.Message }
if ($message -cne 'Payload manifests refuse reparse points.' -or (Test-Path -LiteralPath $linkedOutput)) {
    throw "Native reparse-point refusal is absent: $message"
}
Write-Output 'PASS: existing result, self-inventory and actual native junction refusals preserve fixtures.'
