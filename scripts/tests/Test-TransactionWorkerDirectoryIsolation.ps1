<# A malicious inherited fixture-path variable must not redirect a termination
   worker away from the directory explicitly selected by its parent process. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TestExecutable,
    [Parameter(Mandatory)][string]$EvidenceDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/ArtifactToolProcess.psm1')
$executable = (Get-Item -LiteralPath $TestExecutable -ErrorAction Stop).FullName
$evidence = (Get-Item -LiteralPath $EvidenceDirectory -ErrorAction Stop).FullName
$caseIdentifier = [guid]::NewGuid().ToString('N')
$authorized = Join-Path $evidence "$caseIdentifier-authorized"
$decoy = Join-Path $evidence "$caseIdentifier-decoy"
foreach ($directory in @($authorized, $decoy)) {
    [void](New-Item -ItemType Directory -Path $directory)
    [void](New-Item -ItemType Directory -Path (Join-Path $directory 'journal-store'))
    $stream = [IO.FileStream]::new((Join-Path $directory 'scope-marker.bin'), [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try { $stream.WriteByte($(if ($directory -ceq $authorized) { 0x41 } else { 0x44 })) } finally { $stream.Dispose() }
}
[void](Invoke-ArtifactToolProcess -Executable $executable -WorkingDirectory $authorized `
        -LogDirectory $evidence -LogName "$caseIdentifier-transaction-scope" `
        -Arguments @('[termination-fixture-isolation]', '--reporter', "JUnit::out=$(Join-Path $evidence "$caseIdentifier-transaction-scope.junit.xml")") `
        -EnvironmentOverrides @{
        JPG_SPINNER_TERMINATION_FIXTURE = $decoy
        JPG_SPINNER_TERMINATION_DISPOSITION = 'copy'
    })
foreach ($directory in @($authorized, $decoy)) {
    $expectedByte = if ($directory -ceq $authorized) { 0x41 } else { 0x44 }
    $actualBytes = [IO.File]::ReadAllBytes((Join-Path $directory 'scope-marker.bin'))
    if ($actualBytes.Length -ne 1 -or $actualBytes[0] -ne $expectedByte) { throw 'A directory-isolation marker changed.' }
}
Write-Output 'PASS: ambient path poisoning cannot redirect the worker; authorized and decoy files remain intact.'
