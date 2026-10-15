<# Owned child used only by bounded process-lifecycle regression tests. #>
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Complete','Fail','Timeout')][string]$Outcome)
[Console]::Out.WriteLine("owned-artifact-child:$PID")
[Console]::Error.WriteLine('owned-artifact-stderr')
if ($Outcome -ceq 'Timeout') { Start-Sleep -Seconds 30 }
if ($Outcome -ceq 'Fail') { exit 7 }
exit 0
