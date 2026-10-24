<# Single policy loader and unit boundary for engineering qualification.
   Native JSON/schema consumers own syntax, duplicate-member rejection and
   types. Immutable JsonElement snapshots prevent a caller from mutating the
   policy observed by another consumer in the same PowerShell process. #>
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1')

function ConvertTo-QualificationPolicy {
    <# Validate raw JSON before exposing a native immutable policy snapshot. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$JsonText)
    [void](ConvertFrom-JsonWithUniqueObjectMembers -JsonText $JsonText -RequiredRootValueKind Object -SourceDescription 'Qualification policy')
    $schema = Join-Path (Split-Path -Parent $PSScriptRoot) 'eng/qualification-policy.schema.json'
    if (-not (Test-Json -Json $JsonText -SchemaFile $schema -ErrorAction Stop)) { throw 'Qualification policy does not conform to its native schema.' }
    $document = [Text.Json.JsonDocument]::Parse($JsonText)
    try { return $document.RootElement.Clone() }
    finally { $document.Dispose() }
}

function Read-QualificationPolicy {
    <#
    .SYNOPSIS
    Read one validated snapshot without overrides, repair or host mutation.
    #>
    [CmdletBinding()]
    param([string]$Path = (Join-Path (Split-Path -Parent $PSScriptRoot) 'eng/qualification-policy.json'))
    return ConvertTo-QualificationPolicy -JsonText ([IO.File]::ReadAllText($Path))
}

$script:qualificationPolicy = Read-QualificationPolicy

function Get-QualificationPolicy {
    <#
    .SYNOPSIS
    Return this module's immutable, validated snapshot.
    .DESCRIPTION
    A process uses one coherent policy even if an author edits the source file
    during its run.
    #>
    [CmdletBinding()]
    param()
    return $script:qualificationPolicy
}

function Get-QualificationTimeout {
    <#
    .SYNOPSIS
    Resolve a named category in the requested native API unit.
    .DESCRIPTION
    Whole-minute consumers must not silently round a seconds-based budget.
    Schema bounds guarantee that milliseconds fit a signed native Int32.
    #>
    [CmdletBinding()]
    [OutputType([int])]
    param(
        [Parameter(Mandatory)][string]$Category,
        [ValidateSet('Seconds', 'Milliseconds', 'Minutes')][string]$Unit = 'Seconds',
        [Text.Json.JsonElement]$Policy = (Get-QualificationPolicy)
    )
    # A supplied JsonElement can preserve duplicate members and need not have
    # come from Read-QualificationPolicy. Admit it through the same raw boundary.
    if ($PSBoundParameters.ContainsKey('Policy')) {
        $Policy = ConvertTo-QualificationPolicy -JsonText $Policy.GetRawText()
    }
    # GetDecimal accepts JSON's valid integral exponent/decimal representations;
    # schema validation already excludes fractions, strings and out-of-range values.
    $seconds = [int]$Policy.GetProperty('timeoutSeconds').GetProperty($Category).GetDecimal()
    $duration = [TimeSpan]::FromSeconds($seconds)
    switch ($Unit) {
        'Seconds' { return $seconds }
        'Milliseconds' { return [int]$duration.TotalMilliseconds }
        'Minutes' {
            if ($seconds % 60 -ne 0) { throw "Qualification category must use whole minutes for this consumer: $Category" }
            return [int]$duration.TotalMinutes
        }
    }
}

Export-ModuleMember -Function Read-QualificationPolicy, Get-QualificationPolicy, Get-QualificationTimeout
