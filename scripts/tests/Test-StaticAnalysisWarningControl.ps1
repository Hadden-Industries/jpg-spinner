<# A real native C6011 must fail the actual analysis driver. Compiler /WX
   alone does not establish the later MSBuild diagnostic-ingestion behavior. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Analyzer warning control output must be fresh.' }
$output = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'scripts/Invoke-StaticAnalysis.ps1') `
    -AnalysisScope WarningControl -OutputDirectory $OutputDirectory 2>&1
$exitCode=$LASTEXITCODE
$output | Write-Output
if (($output -join "`n") -notmatch '\bC6011\b') { throw 'Native warning fixture did not reach its C6011 diagnostic.' }
if ($exitCode -eq 0) { throw 'Analysis driver accepted a real native C6011 warning.' }
Write-Output 'PASS: real native C6011 causes analysis driver failure; warning was not merely printed.'
