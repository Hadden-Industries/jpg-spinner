#requires -Version 7.6

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$jsonValidationModulePath =
    Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1'
if (-not (Test-Path -LiteralPath $jsonValidationModulePath -PathType Leaf)) {
    throw "The JSON validation module is absent: $jsonValidationModulePath"
}
Import-Module -Name $jsonValidationModulePath -Force

# Break caught: converting before enforcing the root contract can erase the
# distinction between an allowed object document and an array-shaped document.
$wrongRootKindWasRejected = $false
try {
    [void](ConvertFrom-JsonWithUniqueObjectMembers `
        -JsonText '[{"name":"libjpeg-turbo"}]' `
        -SourceDescription 'array-shaped manifest fixture' `
        -RequiredRootValueKind Object `
        -AsHashtable)
}
catch [System.FormatException] {
    $wrongRootKindWasRejected = $true
    if ($_.Exception.Message.IndexOf(
            'array-shaped manifest fixture root must be a JSON object',
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "JSON root validation failed without the required diagnostic: $($_.Exception.Message)"
    }
}

if (-not $wrongRootKindWasRejected) {
    throw 'JSON validation accepted an array where the caller required an object root.'
}

# Break caught: maintaining a second recursive object-member algorithm can
# drift from the JSON implementation that actually performs deserialization.
# .NET 10 supplies duplicate rejection and a structured path directly.
$duplicateObjectMemberWasRejectedBySystemTextJson = $false
try {
    [void](ConvertFrom-JsonWithUniqueObjectMembers `
        -JsonText '{"metadata":{"version":"1.0","version":"2.0"}}' `
        -SourceDescription 'duplicate-member metadata fixture' `
        -RequiredRootValueKind Object `
        -AsHashtable)
}
catch [System.FormatException] {
    $duplicateObjectMemberWasRejectedBySystemTextJson = $true
    if ($_.Exception.Message.IndexOf(
            'duplicate-member metadata fixture',
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "Duplicate JSON rejection lost its source description: $($_.Exception.Message)"
    }
    if ($_.Exception.InnerException -isnot [System.Text.Json.JsonException]) {
        throw 'Duplicate JSON rejection did not preserve the System.Text.Json exception as its cause.'
    }
    if ([string]::IsNullOrWhiteSpace($_.Exception.InnerException.Path)) {
        throw (
            'Duplicate JSON rejection did not retain the structured JSON path ' +
            'reported by System.Text.Json.'
        )
    }
}

if (-not $duplicateObjectMemberWasRejectedBySystemTextJson) {
    throw 'JSON validation accepted an object with duplicate member names.'
}

Write-Output (
    'PASS: strict JSON validation enforces root kinds and delegates duplicate-member ' +
    'detection to System.Text.Json before conversion.'
)
