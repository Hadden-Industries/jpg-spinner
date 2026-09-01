#requires -Version 7.6

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$jsonObjectMemberValidationModulePath =
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
Import-Module -Name $jsonObjectMemberValidationModulePath

function Invoke-JsonWebRequestWithUniqueObjectMembers {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [uri]$Uri,

        [Parameter(Mandatory)]
        [System.Collections.IDictionary]$Headers,

        [Parameter(Mandatory)]
        [string]$SourceDescription,

        [Parameter()]
        [System.Text.Json.JsonValueKind]$RequiredRootValueKind =
            [System.Text.Json.JsonValueKind]::Undefined,

        [Parameter()]
        [ValidateRange(1, 1024)]
        [int]$MaximumDepth = 100,

        [Parameter()]
        [ValidateRange(1, 300)]
        [int]$ConnectionTimeoutSeconds = 30,

        [Parameter()]
        [ValidateRange(1, 300)]
        [int]$ResponseDataReadTimeoutSeconds = 30,

        [Parameter()]
        [switch]$AsHashtable
    )

    # Invoke-RestMethod automatically converts JSON into PSCustomObject values,
    # which irreversibly discards duplicate member names. Keep transport and
    # interpretation separate: obtain the textual body first, validate the raw
    # JSON with System.Text.Json, and only then perform PowerShell conversion.
    # ConnectionTimeoutSeconds covers connection establishment;
    # OperationTimeoutSeconds also bounds stalled response-data reads.
    $webResponse = Invoke-WebRequest `
        -Uri $Uri `
        -Method Get `
        -Headers $Headers `
        -ConnectionTimeoutSeconds $ConnectionTimeoutSeconds `
        -OperationTimeoutSeconds $ResponseDataReadTimeoutSeconds

    return ConvertFrom-JsonWithUniqueObjectMembers `
        -JsonText ([string]$webResponse.Content) `
        -SourceDescription $SourceDescription `
        -RequiredRootValueKind $RequiredRootValueKind `
        -MaximumDepth $MaximumDepth `
        -AsHashtable:$AsHashtable
}

Export-ModuleMember -Function Invoke-JsonWebRequestWithUniqueObjectMembers
