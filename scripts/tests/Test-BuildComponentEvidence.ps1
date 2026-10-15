<# Actual native exports and the application's resolved lock must retain the
   independent 21 triplet-qualified native and 17 NuGet package requirements.
   This is not a signature, build-execution or runtime-composition test. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][hashtable]$NativeEvidenceDirectories,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$lock=Join-Path $repositoryRoot 'src/JpgSpinner.App/packages.lock.json'
$lockHash=(Get-FileHash -LiteralPath $lock).Hash
$fixture=Join-Path $OwnedOutputRoot ('build-components-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$result=& (Join-Path $repositoryRoot 'scripts/New-JpgSpinnerBuildComponentEvidence.ps1') -NativeEvidenceDirectories $NativeEvidenceDirectories `
    -NuGetLockPath $lock -OutputDirectory (Join-Path $fixture 'evidence') -WorkingDirectory (Join-Path $fixture 'working')
$contract=Get-Content -LiteralPath $result.ComponentContractPath -Raw | ConvertFrom-Json -AsHashtable
$nuget=@($contract.requiredPackages | Where-Object {-not $_.Contains('packageUrl')})
$expected=@(
    'Microsoft.Web.WebView2@1.0.3719.77','Microsoft.Windows.AI.MachineLearning@2.1.74'
    'Microsoft.Windows.CppWinRT@3.0.260818.1','Microsoft.Windows.ImplementationLibrary@1.0.260126.7'
    'Microsoft.Windows.SDK.BuildTools.MSIX@1.7.251221100','Microsoft.Windows.SDK.BuildTools@10.0.28000.2705'
    'Microsoft.WindowsAppSDK.AI@2.5.5','Microsoft.WindowsAppSDK.Base@2.0.4','Microsoft.WindowsAppSDK.DWrite@2.1.0'
    'Microsoft.WindowsAppSDK.Foundation@2.3.12','Microsoft.WindowsAppSDK.InteractiveExperiences@2.1.9'
    'Microsoft.WindowsAppSDK.ML@2.1.94','Microsoft.WindowsAppSDK.Runtime@2.5.1','Microsoft.WindowsAppSDK.Search@2.5.5'
    'Microsoft.WindowsAppSDK.Widgets@2.0.5','Microsoft.WindowsAppSDK.WinUI@2.3.9','Microsoft.WindowsAppSDK@2.5.1'
)
$actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($identity in $nuget) {
    if (-not $actual.Add("$($identity.name)@$($identity.version)")) {throw 'Repeated NuGet target entries were not deduplicated.'}
}
if (-not $actual.SetEquals([string[]]$expected)) {throw "Resolved direct/transitive NuGet coverage differs: $($actual -join ', ')"}
$native=@($contract.requiredPackages | Where-Object {$_.Contains('packageUrl')})
if ($native.Count -ne 21 -or $contract.requiredPackages.Count -ne 38) {throw 'Combined native/NuGet component coverage differs.'}
foreach ($architecture in @('arm64','x64','x86')) {
    $original=Get-Content -LiteralPath (Join-Path $NativeEvidenceDirectories[$architecture] 'required-components.json') -Raw | ConvertFrom-Json -AsHashtable
    foreach ($identity in $original.requiredPackages) {
        $matches=@($native | Where-Object {$_.name -ceq $identity.name -and $_.version -ceq $identity.version -and $_.packageUrl -ceq $identity.packageUrl})
        if ($matches.Count -ne 1) {throw 'An opaque native triplet identity was lost or duplicated.'}
    }
    foreach ($file in Get-ChildItem -LiteralPath $NativeEvidenceDirectories[$architecture] -File -Recurse) {
        $relative=[IO.Path]::GetRelativePath($NativeEvidenceDirectories[$architecture],$file.FullName)
        $copy=Join-Path $result.ComponentEvidenceRoot "native/$architecture/$relative"
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -cne (Get-FileHash -LiteralPath $copy).Hash) {throw 'Native snapshot bytes changed.'}
    }
}
if ((Get-FileHash -LiteralPath $lock).Hash -cne $lockHash -or
    $lockHash -cne (Get-FileHash -LiteralPath (Join-Path $result.ComponentEvidenceRoot 'packages.lock.json')).Hash) {
    throw 'Original NuGet lock was not copied byte-for-byte.'
}
if ([IO.Directory]::Exists((Join-Path $fixture 'working/candidate'))) {throw 'Candidate was not promoted as one complete directory.'}
# A base-target-only enumeration would pass the current package count while
# losing a future RID-only dependency. Keep that scenario explicit and synthetic;
# it does not change the maintained lock or assert the fixture package ships.
$ridLock=Get-Content -LiteralPath $lock -Raw | ConvertFrom-Json -AsHashtable
$ridLock.dependencies['native,Version=v0.0/win-x64']['JpgSpinner.Contract.RidOnly']=@{
    type='Transitive';resolved='1.2.3';contentHash='synthetic-resolved-content-hash'
}
$ridLockPath=Join-Path $fixture 'rid-only.lock.json'
$ridLock | ConvertTo-Json -Depth 50 | Out-File -LiteralPath $ridLockPath -Encoding utf8NoBOM -NoClobber
$ridResult=& (Join-Path $repositoryRoot 'scripts/New-JpgSpinnerBuildComponentEvidence.ps1') -NativeEvidenceDirectories $NativeEvidenceDirectories `
    -NuGetLockPath $ridLockPath -OutputDirectory (Join-Path $fixture 'rid-evidence') -WorkingDirectory (Join-Path $fixture 'rid-working')
$ridContract=Get-Content -LiteralPath $ridResult.ComponentContractPath -Raw | ConvertFrom-Json -AsHashtable
$ridOnly=@($ridContract.requiredPackages | Where-Object {$_.name -ceq 'JpgSpinner.Contract.RidOnly' -and $_.version -ceq '1.2.3'})
if ($ridOnly.Count -ne 1 -or $ridContract.requiredPackages.Count -ne 39) {throw 'A RID-only resolved dependency was omitted or repeated.'}
Write-Output 'PASS: 21 native triplet identities plus all 17 direct/transitive NuGet packages, original snapshot/lock bytes and no partial publication.'
Write-Output 'PASS: synthetic RID-only transitive dependency remains a single additional build-context requirement.'
Write-Output $result
