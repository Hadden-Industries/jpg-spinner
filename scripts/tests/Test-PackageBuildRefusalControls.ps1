<# Refusal checks must stop before MSBuild or any output-directory mutation. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$powershell = (Get-Process -Id $PID).Path
$sentinel = Join-Path $root 'scripts/Invoke-Build.ps1'
$before = (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash
foreach ($case in @(
    @{EntryPoint='ModernSolution'; Diagnostic='requires the PackagedApplication'},
    @{EntryPoint='PackagedApplication'; Diagnostic='must be fresh and absent'}
)) {
    # Existing repository directory is a read-only sentinel, never a package
    # output. Each case must refuse before attempting native build or writes.
    $output = & $powershell -NoProfile -File $sentinel -Configuration Debug -Architecture x64 `
        -BuildEntryPoint $case.EntryPoint -UnsignedPackageOutputDirectory $root 2>&1
    if ($LASTEXITCODE -eq 0 -or ($output -join "`n").IndexOf($case.Diagnostic, [StringComparison]::Ordinal) -lt 0) {
        throw "Package output refusal was absent or imprecise: $($output -join "`n")"
    }
}
foreach ($case in @(
    @{Configuration='Debug'; EntryPoint='PackagedApplication'; Output=$root},
    @{Configuration='Release'; EntryPoint='ModernSolution'; Output=$root},
    @{Configuration='Release'; EntryPoint='PackagedApplication'; Output=''}
)) {
    $arguments=@('-NoProfile','-File',$sentinel,'-Configuration',$case.Configuration,'-Architecture','x64',
        '-BuildEntryPoint',$case.EntryPoint,'-PrepareStoreSubmissionPackage')
    if($case.Output){$arguments+=@('-UnsignedPackageOutputDirectory',$case.Output)}
    $output=& $powershell @arguments 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n").IndexOf('Store submission packaging requires Release, PackagedApplication and an unsigned package output directory.',[StringComparison]::Ordinal) -lt 0){
        throw "Store output prerequisite refusal was absent or imprecise: $($output -join "`n")"
    }
}
if ((Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne $before) {
    throw 'Package-output refusal changed the existing source sentinel.'
}
Write-Output 'PASS: unsigned packaging and Store prerequisites refuse invalid selection/output before native effects; source sentinel unchanged.'
