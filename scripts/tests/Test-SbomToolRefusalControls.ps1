<# Prove a mismatched validator is refused before output or native execution.
   The synthetic text file is intentionally not a valid validator. Neither fake
   executable path may be reached, and neither output directory may be created. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$wrongInput = Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/payload/fixture.txt'
$cases = @(
    @{ Name='schema'; Schema=$wrongInput; Jar=$SpdxToolsJarPath },
    @{ Name='verifier'; Schema=$SpdxSchemaPath; Jar=$wrongInput }
)
foreach ($case in $cases) {
    $output = Join-Path $OutputDirectory $case.Name
    if (Test-Path -LiteralPath $output) { throw 'Refusal-control output must be absent.' }
    $refused = $false
    try {
        & (Join-Path $PSScriptRoot 'Test-SbomToolContract.ps1') `
            -GoExecutablePath 'must-not-execute-go.exe' -JavaExecutablePath 'must-not-execute-java.exe' `
            -ModuleCacheDirectory (Join-Path $output 'modules') -BuildCacheDirectory (Join-Path $output 'build') `
            -OutputDirectory $output -SpdxSchemaPath $case.Schema -SpdxToolsJarPath $case.Jar -SchemaValidatorToolsDirectory 'must-not-use-validator-tools'
    }
    catch {
        if (-not $_.Exception.Message.StartsWith('SPDX validator identity differs from the reviewed pin:', [StringComparison]::Ordinal)) { throw }
        $refused = $true
    }
    if (-not $refused -or (Test-Path -LiteralPath $output)) { throw "Mismatched $($case.Name) validator was not refused before execution/output." }
}
Write-Output 'PASS: mismatched schema and verifier pins refuse before native execution or output creation.'
