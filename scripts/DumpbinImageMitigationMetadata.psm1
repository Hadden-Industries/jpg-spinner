Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertFrom-DumpbinImageMitigationMetadata {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$DumpbinOutput
    )

    # DUMPBIN renders each characteristic and Guard Flag on its own indented
    # line. Anchor every match to that line shape so filenames, diagnostics, or
    # other incidental prose cannot be mistaken for binary mitigation evidence.
    $fieldLineRegexOptions =
        [System.Text.RegularExpressions.RegexOptions]::CultureInvariant -bor
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor
        [System.Text.RegularExpressions.RegexOptions]::Multiline

    return [pscustomobject]@{
        PSTypeName = 'JpgSpinner.DumpbinImageMitigationMetadata'
        advertisesControlFlowGuard = [regex]::IsMatch(
            $DumpbinOutput,
            '^[ \t]+Control Flow Guard[ \t]*\r?$',
            $fieldLineRegexOptions
        )
        isControlFlowGuardInstrumented = [regex]::IsMatch(
            $DumpbinOutput,
            '^[ \t]+CF instrumented[ \t]*\r?$',
            $fieldLineRegexOptions
        )
        hasControlFlowGuardFunctionIdTable = [regex]::IsMatch(
            $DumpbinOutput,
            '^[ \t]+FID table present[ \t]*\r?$',
            $fieldLineRegexOptions
        )
        isCetCompatible = [regex]::IsMatch(
            $DumpbinOutput,
            '^[ \t]+CET compatible[ \t]*\r?$',
            $fieldLineRegexOptions
        )
    }
}

Export-ModuleMember -Function ConvertFrom-DumpbinImageMitigationMetadata
