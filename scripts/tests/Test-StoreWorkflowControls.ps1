<# Challenge the actual protected workflow source in owned copies. Native YAML,
   actionlint and PowerShell parsing remain real; no workflow job is executed. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory,[Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$contract=Join-Path $PSScriptRoot Test-StoreWorkflowContract.ps1
$source=[IO.File]::ReadAllText((Join-Path $root .github/workflows/store-package.yml)).Replace("`r`n","`n")
& $contract -ToolsDirectory $ToolsDirectory
$fixture=Join-Path $OwnedOutputRoot ('store-workflow-controls-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$cases=@(
    @{Name='wrong-environment';Before='environment: store-submission';After='environment: interactive-qualification';Expected='escaped protected admission'},
    @{Name='wrong-admission-role';Before='-TargetEnvironmentName store-submission';After='-TargetEnvironmentName interactive-qualification';Expected='explicit current environment role'},
    @{Name='permissive-download';Before='digest-mismatch: error';After='digest-mismatch: warn';Expected='fail on a digest mismatch'},
    @{Name='signer-before-producer';Before='needs: package';After='needs: admission';Expected='precedes successful package qualification'},
    @{Name='missing-comparison';Before='Invoke-IndependentPackageBuildComparison.ps1';After='Unqualified-PackageComparison.ps1';Expected='ordered boundary'},
    @{Name='privileged-compiler';Before="    permissions:`n      contents: read`n    steps:";After="    permissions:`n      contents: read`n      id-token: write`n    steps:";Expected='trusted-main hosted native publisher'},
    @{Name='compiler-in-publisher';Before='      - uses: actions/attest@';After="      - run: ./scripts/New-StoreSubmissionSbom.ps1`n      - uses: actions/attest@";Expected='publisher must not compile'}
)
foreach($case in $cases){
    if(-not $source.Contains($case.Before,[StringComparison]::Ordinal)){throw "Store control lacks its actual source precondition: $($case.Name)"}
    $path=Join-Path $fixture ($case.Name+'.yml')
    [IO.File]::WriteAllText($path,$source.Replace($case.Before,$case.After),[Text.UTF8Encoding]::new($false))
    $refusal=$null
    try {& $contract -ToolsDirectory $ToolsDirectory -WorkflowPath $path | Out-Null} catch {$refusal=$_.Exception.Message}
    if($null -eq $refusal -or -not $refusal.Contains($case.Expected,[StringComparison]::Ordinal)){throw "Store control was not refused at its intended boundary: $($case.Name): $refusal"}
    Write-Output "PASS: Store source refusal: $($case.Name)"
}
