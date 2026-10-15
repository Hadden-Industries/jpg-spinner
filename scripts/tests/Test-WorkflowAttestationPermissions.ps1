<# Exercise the signing-permission boundary through the real native YAML
   consumer. These inert fixtures are never dispatched or executed. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory,[Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$policy=Join-Path $root 'scripts/Test-GitHubWorkflowPolicy.ps1'
$text=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'WorkflowFixtures/Valid.yml'))
$signed=$text.Replace('  verify:',@'
  verify:
    if: github.ref == 'refs/heads/main'
    permissions:
      contents: read
      id-token: write
      attestations: write
'@).Replace("      - run: Write-Output 'Static fixture only; never executed by its validator.'",@'
      - uses: actions/attest@1e69f48acb82d1966a394da916b4c1698aa569d6 # v4.2.2
        with:
          subject-path: inputs.json
          predicate-type: https://haddenindustries.com/schemas/jpg-spinner/interactive-test-inputs/v1
          predicate-path: inputs.json
'@)
$output=Join-Path $OwnedOutputRoot ('attestation-permissions-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($output)
$cases=@(
    @{Name='trusted-hosted';Text=$signed;Accepted=$true},
    @{Name='untrusted-ref';Text=$signed.Replace("github.ref == 'refs/heads/main'","github.ref == 'refs/heads/development'");Accepted=$false},
    @{Name='no-native-publisher';Text=$signed.Replace('actions/attest@1e69f48acb82d1966a394da916b4c1698aa569d6','actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9');Accepted=$false},
    @{Name='self-hosted';Text=$signed.Replace('runs-on: windows-2025-vs2026',"runs-on: [self-hosted, Windows, X64, jpg-spinner-interactive]`n    environment: interactive-qualification");Accepted=$false}
)
foreach ($case in $cases) {
    $path=Join-Path $output ($case.Name+'.yml')
    [IO.File]::WriteAllText($path,$case.Text)
    $message=$null
    try {& $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($path)} catch {$message=$_.Exception.Message}
    if ($case.Accepted) {
        if ($null -ne $message) {throw "Native publisher fixture refused: $message"}
    } elseif ($null -eq $message -or -not $message.Contains('trusted-main hosted native publisher',[StringComparison]::Ordinal)) {
        throw "Signing permission escaped its intended boundary: $($case.Name): $message"
    }
    Write-Output "PASS attestation permission control: $($case.Name)"
}
