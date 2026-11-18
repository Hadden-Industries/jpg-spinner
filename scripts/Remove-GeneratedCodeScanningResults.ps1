<# Remove CodeQL results located only in C++/WinRT projection headers before upload.
   cppwinrt.exe writes Microsoft's library and projections from Windows metadata
   into each project's "Generated Files/winrt" directory. They are not repository
   source, and their findings are characterized in
   docs/qualification/2026-10-26-codeql-remediation.md. XAML-generated sources
   ("Generated Files/*.g.h" from this repository's markup) are retained.
   Every other result, rule, artifact and run property is preserved unchanged. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SarifDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$directory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SarifDirectory)
$files = @(Get-ChildItem -LiteralPath $directory -Filter '*.sarif' -File)
if ($files.Count -eq 0) { throw "No SARIF output exists in $directory." }

function Test-GeneratedProjectionUri {
    param([AllowNull()][string]$Uri)
    if ([string]::IsNullOrEmpty($Uri)) { return $false }
    $path = [Uri]::UnescapeDataString($Uri).Replace('\', '/')
    return $path.IndexOf('/Generated Files/winrt/', [StringComparison]::Ordinal) -ge 0 -or
        $path.StartsWith('Generated Files/winrt/', [StringComparison]::Ordinal)
}

function Get-PrimaryUri {
    param([System.Text.Json.Nodes.JsonNode]$Result, [System.Text.Json.Nodes.JsonArray]$Artifacts)
    $locations = $Result['locations']
    if ($null -eq $locations -or $locations.Count -eq 0) { return $null }
    $physicalLocation = $locations[0]['physicalLocation']
    if ($null -eq $physicalLocation) { return $null }
    $artifactLocation = $physicalLocation['artifactLocation']
    if ($null -eq $artifactLocation) { return $null }
    if ($null -ne $artifactLocation['uri']) { return $artifactLocation['uri'].GetValue[string]() }
    $index = $artifactLocation['index']
    if ($null -eq $index -or $null -eq $Artifacts) { return $null }
    $artifact = $Artifacts[$index.GetValue[int]()]
    if ($null -eq $artifact -or $null -eq $artifact['location'] -or $null -eq $artifact['location']['uri']) { return $null }
    return $artifact['location']['uri'].GetValue[string]()
}

$documentOptions = [System.Text.Json.JsonDocumentOptions]::new()
$writeOptions = [System.Text.Json.JsonSerializerOptions]::new()
$removedTotal = 0
foreach ($file in $files) {
    $sarif = [System.Text.Json.Nodes.JsonNode]::Parse([IO.File]::ReadAllText($file.FullName), $null, $documentOptions)
    if ($null -eq $sarif -or $sarif['runs'] -isnot [System.Text.Json.Nodes.JsonArray]) {
        throw "SARIF output lacks a runs array: $($file.FullName)"
    }
    foreach ($run in $sarif['runs']) {
        $results = $run['results']
        if ($null -eq $results) { continue }
        $artifacts = $run['artifacts']
        for ($index = $results.Count - 1; $index -ge 0; $index--) {
            if (Test-GeneratedProjectionUri -Uri (Get-PrimaryUri -Result $results[$index] -Artifacts $artifacts)) {
                $results.RemoveAt($index)
                $removedTotal++
            }
        }
    }
    [IO.File]::WriteAllText($file.FullName, $sarif.ToJsonString($writeOptions), [Text.UTF8Encoding]::new($false))
}
Write-Output "Removed $removedTotal result(s) located in generated C++/WinRT projection headers."
