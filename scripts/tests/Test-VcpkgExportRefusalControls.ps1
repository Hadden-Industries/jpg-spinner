<# Existing paths and a wrong executable pin must refuse before export; an
   actual empty installed tree must fail the native exporter without promotion.
   No fake native success response or modified original build tree is used. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot,[Parameter(Mandatory)][string]$VcpkgRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('export-refusal-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$empty=Join-Path $fixture 'empty-installed'
$wrongTool=Join-Path $fixture 'wrong-tool'
foreach ($directory in @($empty,$wrongTool)) {[void][IO.Directory]::CreateDirectory($directory)}
[IO.File]::Copy((Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/payload/fixture.txt'),(Join-Path $wrongTool 'vcpkg.exe'),$false)
$producer=Join-Path $repositoryRoot 'scripts/Export-JpgSpinnerVcpkgEvidence.ps1'
foreach ($case in @(
    @{Name='existing-output';Tool=$VcpkgRoot;Output=$fixture;Diagnostic='fresh and absent'},
    @{Name='wrong-pin';Tool=$wrongTool;Output=(Join-Path $fixture 'wrong-pin-components');Diagnostic='exporter identity differs'}
)) {
    $working=Join-Path $fixture "$($case.Name)-work"
    $message=$null
    try {& $producer -Architecture x64 -InstalledRoot $empty -VcpkgRoot $case.Tool -EvidenceDirectory $case.Output -WorkingDirectory $working | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal) -or (Test-Path -LiteralPath $working)) {throw "Preflight refusal '$($case.Name)' failed or had effects: $message"}
}
$working=Join-Path $fixture 'native-refusal-work'
$output=Join-Path $fixture 'native-refusal-components'
$message=$null
$successValues=@()
try {$successValues=@(& $producer -Architecture x64 -InstalledRoot $empty -VcpkgRoot $VcpkgRoot -EvidenceDirectory $output -WorkingDirectory $working)}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.StartsWith('Native vcpkg refused installed-export',[StringComparison]::Ordinal) -or
    $successValues.Count -ne 0 -or (Test-Path -LiteralPath $output) -or -not [IO.File]::Exists((Join-Path $working 'installed-export.stderr.log'))) {
    throw "Native absent-root refusal was missing or promoted partial evidence: $message"
}
Write-Output 'PASS: existing output and wrong exporter pin refuse before effects; actual absent installed roots fail natively without evidence promotion.'
