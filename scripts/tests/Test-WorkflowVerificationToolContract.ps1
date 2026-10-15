<# Qualify actual validator wiring against independently authored YAML.
   Positive syntax must pass; duplicate keys must fail before lossy conversion. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tools = & (Join-Path $repositoryRoot 'scripts/Install-WorkflowVerificationTools.ps1') -ToolsDirectory $ToolsDirectory
Add-Type -Path $tools.yamlAssemblyPath
if ([Diagnostics.FileVersionInfo]::GetVersionInfo($tools.yamlAssemblyPath).ProductVersion -cne '18.1.0') {
    throw 'The selected YAML assembly identity differs from the reviewed release.'
}
# The release intentionally retains AssemblyVersion 18.0.0.0 for its ABI;
# package/product identity is not the CLR assembly binding version.
$document = [YamlDotNet.RepresentationModel.YamlStream]::new()
$reader = [IO.File]::OpenText((Join-Path $PSScriptRoot 'WorkflowFixtures/Valid.yml'))
try { $document.Load($reader) } finally { $reader.Dispose() }
if ($document.Documents.Count -ne 1 -or $document.Documents[0].RootNode.NodeType.ToString() -cne 'Mapping') {
    throw 'The positive YAML fixture is not the expected actual mapping document.'
}
$reader = [IO.File]::OpenText((Join-Path $PSScriptRoot 'WorkflowFixtures/DuplicateKey.yml'))
$message = $null
try { $document.Load($reader) } catch { $message = $_.Exception.Message } finally { $reader.Dispose() }
if ($null -eq $message -or -not $message.Contains('Duplicate key',[StringComparison]::Ordinal)) {
    throw 'The native YAML model did not reject duplicate mapping keys.'
}
& $tools.actionlintExecutablePath -color=false (Join-Path $PSScriptRoot 'WorkflowFixtures/Valid.yml')
if ($LASTEXITCODE -ne 0) { throw 'The actual native Actions validator refused the positive workflow fixture.' }
& $tools.actionlintExecutablePath -color=false (Join-Path $PSScriptRoot 'WorkflowFixtures/InvalidExpression.yml')
if ($LASTEXITCODE -eq 0) { throw 'The native Actions validator accepted an undefined expression context.' }
Write-Output 'PASS: exact native validator identities, actual YAML mapping/duplicate rejection and Actions syntax.'
