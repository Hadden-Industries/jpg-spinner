<# A real fresh native probe rebuild must emit analysis, not merely compile.
   Public MSBuild owns /analyze integration; no command-line shadow parser. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Analysis qualification outputs must be fresh.' }
& (Join-Path $root 'scripts/Invoke-StaticAnalysis.ps1') -AnalysisScope ToolchainProbe -OutputDirectory $OutputDirectory
$report = Join-Path $OutputDirectory 'obj/BuildPolicyProbe.nativecodeanalysis.xml'
if (-not (Test-Path -LiteralPath $report -PathType Leaf) -or (Get-Item -LiteralPath $report).Length -eq 0) {
    throw 'Native rebuild did not emit the fresh probe code-analysis report.'
}
if (-not (Test-Path -LiteralPath (Join-Path $OutputDirectory 'StaticAnalysis.binlog') -PathType Leaf)) {
    throw 'Native analysis binary log is absent.'
}
$xml = [Xml.XmlDocument]::new()
$xml.Load($report)
if ($null -eq $xml.DocumentElement) { throw 'Native analysis report is not a parsed XML document.' }
Write-Output 'PASS: actual fresh native analyzer report and binary log; not whole-application analysis.'
