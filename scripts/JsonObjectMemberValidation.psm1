#requires -Version 7.6

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertFrom-JsonWithUniqueObjectMembers {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$JsonText,

        [Parameter(Mandatory)]
        [string]$SourceDescription,

        [Parameter()]
        [System.Text.Json.JsonValueKind]$RequiredRootValueKind =
            [System.Text.Json.JsonValueKind]::Undefined,

        [Parameter()]
        [switch]$AsHashtable,

        [Parameter()]
        [ValidateRange(1, 1024)]
        [int]$MaximumDepth = 100
    )

    # PowerShell's ConvertFrom-Json intentionally retains only the last
    # identically named member. PowerShell 7.6 runs on .NET 10, whose JSON
    # serializer can reject duplicates itself. Let the platform parser enforce
    # JSON grammar, depth, member uniqueness, and the optional root contract;
    # only then perform PowerShell's convenient but lossy object conversion.
    $jsonSerializerOptions = [System.Text.Json.JsonSerializerOptions]::new()
    $jsonSerializerOptions.AllowDuplicateProperties = $false
    $jsonSerializerOptions.AllowTrailingCommas = $false
    $jsonSerializerOptions.ReadCommentHandling = [System.Text.Json.JsonCommentHandling]::Disallow
    $jsonSerializerOptions.MaxDepth = $MaximumDepth

    try {
        $jsonRootElement = [System.Text.Json.JsonSerializer]::Deserialize(
            $JsonText,
            [System.Text.Json.JsonElement],
            $jsonSerializerOptions
        )

        if (
            $RequiredRootValueKind -ne [System.Text.Json.JsonValueKind]::Undefined -and
            $jsonRootElement.ValueKind -ne $RequiredRootValueKind
        ) {
            $requiredRootKindName = $RequiredRootValueKind.ToString().ToLowerInvariant()
            throw [System.FormatException]::new(
                "$SourceDescription root must be a JSON $requiredRootKindName."
            )
        }
    }
    catch [System.Text.Json.JsonException] {
        # Retain the platform exception as the machine-readable cause (Path,
        # line, and byte position) while adding the semantic source name that
        # callers use in repository and live-response diagnostics.
        throw [System.FormatException]::new(
            "$SourceDescription is not valid strict JSON: $($_.Exception.Message)",
            $_.Exception
        )
    }

    $conversionParameters = @{
        InputObject = $JsonText
        Depth = $MaximumDepth
    }
    if ($AsHashtable.IsPresent) {
        $conversionParameters['AsHashtable'] = $true
    }

    return ConvertFrom-Json @conversionParameters
}

Export-ModuleMember -Function ConvertFrom-JsonWithUniqueObjectMembers
