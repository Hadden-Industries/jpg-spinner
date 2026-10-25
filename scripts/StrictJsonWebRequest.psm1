#requires -Version 7.6

Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1') -ErrorAction Stop
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
        [int]$ConnectionTimeoutSeconds = (Get-QualificationTimeout -Category jsonTransport),

        [Parameter()]
        [ValidateRange(1, 300)]
        [int]$ResponseDataReadTimeoutSeconds = (Get-QualificationTimeout -Category jsonTransport),

        [Parameter()]
        [switch]$AsHashtable,

        [Parameter()]
        [switch]$FollowPagination,

        [Parameter()]
        [ValidateRange(1, 1000)]
        [int]$MaximumPageCount = 100
    )

    # Invoke-RestMethod automatically converts JSON into PSCustomObject values,
    # which irreversibly discards duplicate member names. Keep transport and
    # interpretation separate: obtain the textual body first, validate the raw
    # JSON with System.Text.Json, and only then perform PowerShell conversion.
    # Qualification may encounter a cold proxy or slow service. Longer finite
    # defaults do not retry or accept invalid JSON.
    # ConnectionTimeoutSeconds covers connection establishment;
    # OperationTimeoutSeconds also bounds stalled response-data reads.
    $nextPageUri = $Uri
    $parsedPages = [Collections.Generic.List[object]]::new()
    for ($pageNumber = 1; $pageNumber -le $MaximumPageCount; $pageNumber++) {
        $webResponse = Invoke-WebRequest `
            -Uri $nextPageUri `
            -Method Get `
            -Headers $Headers `
            -ConnectionTimeoutSeconds $ConnectionTimeoutSeconds `
            -OperationTimeoutSeconds $ResponseDataReadTimeoutSeconds

        $parsedPages.Add((ConvertFrom-JsonWithUniqueObjectMembers `
                    -JsonText ([string]$webResponse.Content) `
                    -SourceDescription $SourceDescription `
                    -RequiredRootValueKind $RequiredRootValueKind `
                    -MaximumDepth $MaximumDepth `
                    -AsHashtable:$AsHashtable))

        # Invoke-WebRequest supplies the native RFC relation-link dictionary.
        # Keep raw-body validation on every page; no JSON auto-deserialization
        # or custom Link-header parser participates in this trust boundary.
        if (-not $FollowPagination -or -not $webResponse.RelationLink.ContainsKey('next')) {
            foreach ($parsedPage in $parsedPages) { $parsedPage }
            return
        }
        $nextPageUri = [uri]$webResponse.RelationLink['next']
    }
    # Never publish a partial successful inventory when a server keeps linking.
    throw "Pagination remains incomplete after $MaximumPageCount pages: $SourceDescription"
}

Export-ModuleMember -Function Invoke-JsonWebRequestWithUniqueObjectMembers
