[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataParserModulePath =
    Join-Path $repositoryRoot 'scripts/DumpbinImageMitigationMetadata.psm1'
if (-not (Test-Path -LiteralPath $metadataParserModulePath -PathType Leaf)) {
    throw "The dumpbin image-mitigation metadata parser module is absent: $metadataParserModulePath"
}
Import-Module -Name $metadataParserModulePath -Force

$completeMitigationOutput = @'
OPTIONAL HEADER VALUES
                   Control Flow Guard

        00000001 extended DLL characteristics
                   CET compatible

            00000500 Guard Flags
                       CF instrumented
                       FID table present
'@
$completeMetadata = ConvertFrom-DumpbinImageMitigationMetadata `
    -DumpbinOutput $completeMitigationOutput
foreach ($requiredBooleanPropertyName in @(
    'advertisesControlFlowGuard',
    'isControlFlowGuardInstrumented',
    'hasControlFlowGuardFunctionIdTable',
    'isCetCompatible'
)) {
    if ($completeMetadata.$requiredBooleanPropertyName -ne $true) {
        throw "Complete dumpbin evidence did not set '$requiredBooleanPropertyName'."
    }
}

# A linker can advertise CFG in the optional header without proving that the
# compiler instrumented the image or emitted its function-ID table. Preserve
# that distinction in the parsed representation.
$headerOnlyMitigationOutput = @'
OPTIONAL HEADER VALUES
                   Control Flow Guard

        00000001 extended DLL characteristics
                   CET compatible
'@
$headerOnlyMetadata = ConvertFrom-DumpbinImageMitigationMetadata `
    -DumpbinOutput $headerOnlyMitigationOutput
if ($headerOnlyMetadata.advertisesControlFlowGuard -ne $true) {
    throw 'Header-only dumpbin evidence must retain the CFG advertisement.'
}
if ($headerOnlyMetadata.isControlFlowGuardInstrumented -ne $false) {
    throw 'Header-only dumpbin evidence must not imply CFG instrumentation.'
}
if ($headerOnlyMetadata.hasControlFlowGuardFunctionIdTable -ne $false) {
    throw 'Header-only dumpbin evidence must not imply a CFG function-ID table.'
}

# Match dumpbin field lines, not incidental prose containing the same terms.
$incidentalTextMetadata = ConvertFrom-DumpbinImageMitigationMetadata -DumpbinOutput @'
Diagnostic: expected CF instrumented output was absent.
Diagnostic: expected FID table present output was absent.
'@
if (
    $incidentalTextMetadata.isControlFlowGuardInstrumented -ne $false -or
    $incidentalTextMetadata.hasControlFlowGuardFunctionIdTable -ne $false
) {
    throw 'Incidental diagnostic prose must not satisfy load-configuration evidence.'
}

Write-Output (
    'PASS: dumpbin mitigation parsing distinguishes image advertisements from ' +
    'CFG instrumentation and function-table evidence.'
)
