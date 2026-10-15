<# Qualify portable native Git SSH trust against an actual signed source commit.
   A command-scoped policy must accept the reviewed public signer and refuse an
   empty policy. This does not create signatures, establish a new trust identity,
   modify Git configuration or authenticate arbitrary caller-supplied policies. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$policy=Join-Path $root eng/source-commit-allowed-signers
$commit=& git -C $root rev-parse HEAD
if($LASTEXITCODE -ne 0){throw 'Cannot select the signed source fixture.'}
$fixture=Join-Path $OwnedOutputRoot ('source-signature-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$empty=Join-Path $fixture empty-signers
[IO.File]::WriteAllText($empty,'')
$git=(Get-Command git -CommandType Application | Select-Object -First 1).Source
Import-Module (Join-Path $root scripts/ArtifactToolProcess.psm1) -Force
$arguments=@('-C',$root,'-c',"gpg.ssh.allowedSignersFile=$policy",'verify-commit',$commit)
Invoke-ArtifactToolProcess -Executable $git -Arguments $arguments -WorkingDirectory $root -LogDirectory $fixture -LogName trusted | Out-Null
$refused=$null
try {
    Invoke-ArtifactToolProcess -Executable $git -WorkingDirectory $root -LogDirectory $fixture -LogName untrusted `
        -Arguments @('-C',$root,'-c',"gpg.ssh.allowedSignersFile=$empty",'verify-commit',$commit) | Out-Null
} catch {$refused=$_.Exception.Message}
if($refused -cne 'Native artifact tool failed: untrusted (exit 1)'){throw "An empty native trust policy did not refuse the real signed commit: $refused"}
Write-Output 'PASS: native Git authenticates actual source with the portable public signer policy and refuses empty trust without configuration changes.'
