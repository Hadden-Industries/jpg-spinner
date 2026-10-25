<# Exercise the public native-child byte contract rather than a text encoder
   standing in for a binary downloader. This creates only owned small fixtures. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1') -Force
$working = Join-Path $OwnedOutputRoot ('artifact-binary-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($working)
$powershell = (Get-Process -Id $PID).Path
$fixture = Join-Path $PSScriptRoot 'Fixtures/ArtifactBinaryOutput.ps1'
$expected = [byte[]]@(0, 255, 128, 195, 40, 13, 10, 65, 0, 254)

foreach ($outcome in @('Complete', 'Fail')) {
    $destination = Join-Path $working "$outcome.bin"
    $arguments = @('-NoProfile', '-File', $fixture)
    if ($outcome -ceq 'Fail') { $arguments += '-Fail' }
    $failure = $null
    try {
        Invoke-ArtifactToolProcess -Executable $powershell -Arguments $arguments `
            -WorkingDirectory $root -LogDirectory $working -LogName $outcome `
            -BinaryStandardOutputPath $destination | Out-Null
    }
    catch { $failure = $_.Exception.Message }
    if ($outcome -ceq 'Complete' -and $null -ne $failure) { throw $failure }
    if ($outcome -ceq 'Fail' -and ($null -eq $failure -or -not $failure.Contains('exit 7', [StringComparison]::Ordinal))) {
        throw "Binary failure did not retain its native exit: $failure"
    }
    $actual = [IO.File]::ReadAllBytes($destination)
    if ([Convert]::ToHexString($actual) -cne [Convert]::ToHexString($expected)) {
        throw 'Native binary stdout changed through text decoding or an added delimiter.'
    }
    if ([IO.File]::ReadAllText((Join-Path $working "$outcome.stderr.log")).Trim() -cne 'owned-binary-child-stderr') {
        throw 'Native binary child stderr was not retained.'
    }
    # CreateNew must refuse an existing file before starting another child.
    $refused = $false
    try {
        Invoke-ArtifactToolProcess -Executable $powershell -Arguments $arguments `
            -WorkingDirectory $root -LogDirectory $working -LogName "$outcome-existing" `
            -BinaryStandardOutputPath $destination | Out-Null
    }
    catch { $refused = $true }
    if (-not $refused -or [Convert]::ToHexString([IO.File]::ReadAllBytes($destination)) -cne [Convert]::ToHexString($expected)) {
        throw 'Binary output overwrote an existing preserved payload.'
    }
}
Write-Output 'PASS: native raw-byte success/failure preservation, stderr and existing-file refusal.'
