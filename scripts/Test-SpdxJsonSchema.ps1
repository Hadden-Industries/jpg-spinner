<# Bound native validation of the unchanged official SPDX schema. The caller
   supplies a qualified private npm-ci tree and has already checked strict JSON
   and the schema digest. No installation, schema repair or fallback occurs.
   SPDX recommends Ajv: https://github.com/spdx/spdx-3-model/blob/develop/serialization/jsonld/validation.md #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SchemaValidatorToolsDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent $PSScriptRoot
$definition=Join-Path $repositoryRoot 'eng/spdx-validation'
$tools=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SchemaValidatorToolsDirectory)
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$pins=ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText((Join-Path $repositoryRoot 'eng/sbom-tools.json'))) `
    -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
$package=ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText((Join-Path $definition 'package.json'))) `
    -RequiredRootValueKind Object -SourceDescription 'SPDX schema validator package definition' -AsHashtable
if ($pins.spdxSchemaValidator.package -cne 'ajv' -or $pins.spdxSchemaValidator.version -cne $package.dependencies.ajv) {
    throw 'SPDX schema validator package differs from the reviewed SBOM tool pin.'
}
if ((Get-FileHash -LiteralPath (Join-Path $definition 'package-lock.json')).Hash -cne
    (Get-FileHash -LiteralPath (Join-Path $tools 'package-lock.json')).Hash -or
    (Get-FileHash -LiteralPath (Join-Path $definition 'package.json')).Hash -cne
    (Get-FileHash -LiteralPath (Join-Path $tools 'package.json')).Hash) {
    throw 'SPDX schema validator tool definitions differ from the reviewed lock.'
}
$start=[Diagnostics.ProcessStartInfo]::new()
$nodeCommands=@(Get-Command node -CommandType Application -ErrorAction Stop)
$start.FileName=$nodeCommands[0].Source
$start.WorkingDirectory=$repositoryRoot
$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
# Child-only settings prevent ambient preload/heap flags from overriding this
# bounded consumer. V8 owns the 512 MiB old-space bound; the deadline and overall
# working-set refusal below additionally bound non-heap memory and native work.
[void]$start.Environment.Remove('NODE_OPTIONS')
[void]$start.Environment.Remove('NODE_PATH')
foreach ($argument in @('--max-old-space-size=512',(Join-Path $definition 'validate-spdx-schema.cjs'),
    '--document',$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ManifestPath),
    '--schema',$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SpdxSchemaPath),'--tools',$tools)) {
    $start.ArgumentList.Add($argument)
}
$process=[Diagnostics.Process]::new();$process.StartInfo=$start
$timer=[Diagnostics.Stopwatch]::StartNew()
try {
    if (-not $process.Start()) {throw 'Native SPDX schema validator did not start.'}
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    while (-not $process.WaitForExit(100)) {
        $process.Refresh()
        if ($timer.Elapsed.TotalSeconds -gt 30 -or $process.WorkingSet64 -gt 1GB) {
            $process.Kill($true)
            if (-not $process.WaitForExit(5000)) {throw 'Native SPDX schema validator termination remains unresolved.'}
            throw 'Native SPDX schema validator exceeded its resource bound.'
        }
    }
    $diagnostic=$stderr.GetAwaiter().GetResult()
    if ($process.ExitCode -eq 1) {throw "Official SPDX 3.0.1 schema rejected native output. $diagnostic"}
    if ($process.ExitCode -ne 0) {throw "Native SPDX schema validator failed (exit $($process.ExitCode)). $diagnostic"}
    Write-Output $stdout.GetAwaiter().GetResult().TrimEnd()
} finally {$process.Dispose()}
