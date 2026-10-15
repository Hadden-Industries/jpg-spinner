<# Exercise the actual native MSBuild/property/file boundary without building.
   A local observation must disclose local-runner identity and no profile paths. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (Test-Path -LiteralPath $OutputPath) { throw 'Actual provenance observation requires a fresh result.' }
& (Join-Path $root 'scripts/New-BuildProvenance.ps1') -Configuration Release -Architecture x64 -OutputPath $OutputPath
& (Join-Path $root 'scripts/Test-BuildProvenance.ps1') -Path $OutputPath
$record = Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json -AsHashtable
$head = & git -C $root rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0 -or $record.sourceCommit -cne $head) { throw 'Observed provenance does not name the actual current source commit.' }
if ($record.runnerImage.label -cne 'local-development' -or $record.runnerImage.version -cne 'not-a-hosted-runner') {
    throw 'The native local qualification must not masquerade as a hosted runner.'
}
if ($record.selectedTools.compiler.path -cnotlike '<visual-studio>/*/cl.exe' -or $record.nugetLocks.Count -lt 1) {
    throw 'Native selected compiler or committed project lock observations are absent.'
}
$text = Get-Content -LiteralPath $OutputPath -Raw
if ($text.Contains([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile),[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Observed provenance leaked the developer profile path.'
}
Write-Output 'PASS: actual selected native tools, current source, project locks and honest redacted local environment.'
