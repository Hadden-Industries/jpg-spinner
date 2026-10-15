<# Native MSIX writers/extractors qualify the closed comparison end to end.
   Copy a retained development payload into owned fixture sources, replacing
   only those copies with native-written metadata. These synthetic packages
   are not shipping, installability, independent-build or certification proof. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('native-payload-comparison-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$originalHash=(Get-FileHash -LiteralPath $BundlePath).Hash
$baseline=Join-Path $fixture baseline
& (Join-Path $root 'scripts/Expand-JpgSpinnerBundle.ps1') -BundlePath $BundlePath -InspectionDirectory $baseline | Out-Null
$toolchain=& (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
Push-Location -LiteralPath $root
try {
    $makeAppx=& $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') -nologo -noAutoResponse -p:Platform=x64 -p:GenerateAppxPackageOnBuild=true '-getProperty:MakeAppxExeFullPath'
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($makeAppx)) {throw 'Fixture cannot resolve the owning native SDK.'}
} finally {Pop-Location}
Import-Module (Join-Path $PSScriptRoot 'Fixtures/WinmdFixture.psm1') -Force
$firstMetadata=New-TestWinmd $fixture first ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 1
$secondMetadata=New-TestWinmd $fixture second ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2
function New-NativeFixtureBundle([string]$Name,[string]$Metadata,[string]$Control='none') {
    $directory=Join-Path $fixture $Name
    [void][IO.Directory]::CreateDirectory($directory)
    $packages=[Collections.Generic.List[string]]::new()
    foreach ($architecture in @('x86','x64','arm64')) {
        $source=Join-Path $directory $architecture
        Copy-Item -LiteralPath (Join-Path $baseline "payloads/$architecture") -Destination $source -Recurse
        Copy-Item -LiteralPath $Metadata -Destination (Join-Path $source JpgSpinner.winmd) -Force
        # A new native package owns its own generated footprint. Exclude only
        # these exact fixture-owned files; never touch the retained baseline.
        foreach ($generated in @('AppxBlockMap.xml','[Content_Types].xml','AppxSignature.p7x')) {
            $member=Join-Path $source $generated
            if (Test-Path -LiteralPath $member) {Remove-Item -LiteralPath $member}
        }
        $probe=Join-Path $source comparison-control.bin
        if ($Control -ne 'omit') {[IO.File]::WriteAllBytes($probe,[byte[]]@(1,2,3,4))}
        if ($architecture -ceq 'x64') {
            if ($Control -ceq 'changed') {[IO.File]::WriteAllBytes($probe,[byte[]]@(1,2,3,5))}
            if ($Control -ceq 'extra') {[IO.File]::WriteAllBytes((Join-Path $source extra-control.bin),[byte[]]@(1))}
        }
        $package=Join-Path $directory ($architecture+'.msix')
        & $makeAppx pack /d $source /p $package /h SHA256 /no *> (Join-Path $directory ($architecture+'-pack.log'))
        if ($LASTEXITCODE -ne 0) {throw "Native fixture packing failed: $directory/$architecture-pack.log"}
        $packages.Add($package)
    }
    $bundle=Join-Path $directory fixture.msixbundle
    & (Join-Path $root 'scripts/New-MsixBundle.ps1') -PackagePaths $packages.ToArray() -OutputPath $bundle -WorkingDirectory (Join-Path $directory staging) *> (Join-Path $directory bundle.log)
    return $bundle
}
$first=New-NativeFixtureBundle first-package $firstMetadata
$second=New-NativeFixtureBundle second-package $secondMetadata
$firstHash=(Get-FileHash -LiteralPath $first).Hash
$secondHash=(Get-FileHash -LiteralPath $second).Hash
if ((Get-FileHash -LiteralPath $first).Hash -ceq (Get-FileHash -LiteralPath $second).Hash) {throw 'Native fixture must contain actual identity variance.'}
$test=Join-Path $root 'scripts/Compare-JpgSpinnerBundlePayload.ps1'
$same=& $test -FirstBundlePath $first -SecondBundlePath $first -WorkingDirectory (Join-Path $fixture identical)
if (-not $same.ComparisonSatisfied -or -not $same.PayloadsByteIdentical -or -not $same.ContainersByteIdentical -or $same.ObservedVariances.Count -ne 0) {throw 'Identical original containers were not accepted truthfully.'}
$different=& $test -FirstBundlePath $first -SecondBundlePath $second -WorkingDirectory (Join-Path $fixture identity-only)
if (-not $different.ComparisonSatisfied -or $different.PayloadsByteIdentical -or $different.ContainersByteIdentical -or $different.ObservedVariances.Count -ne 3) {throw 'Qualified native WinMD/package variance was concealed or refused.'}
if($different.Policy -cne 'jpg-spinner-package-payload-comparison-v2'){throw 'Native comparison did not identify the current approved policy.'}
foreach($variance in $different.ObservedVariances){
    $map=$variance.BlockMapComparison
    if($map.Policy -cne 'jpg-spinner-blockmap-qualified-variance-v2' -or $map.FileOrderVarianceObserved -isnot [bool] -or
        $map.FirstFileOrder.Count -eq 0 -or $map.FirstFileOrder.Count -ne $map.SecondFileOrder.Count){throw 'Native comparison omitted its whole-File order evidence.'}
    $orderChanged=-not [Linq.Enumerable]::SequenceEqual[string]([string[]]$map.FirstFileOrder,[string[]]$map.SecondFileOrder,[StringComparer]::Ordinal)
    if($orderChanged -ne $map.FileOrderVarianceObserved){throw 'Native comparison concealed actual File order variance.'}
}
foreach ($control in @('changed','extra','omit')) {
    $invalid=New-NativeFixtureBundle ($control+'-package') $secondMetadata $control
    $output=Join-Path $fixture ($control+'-comparison')
    $invalidHash=(Get-FileHash -LiteralPath $invalid).Hash
    $message=$null
    try {& $test -FirstBundlePath $first -SecondBundlePath $invalid -WorkingDirectory $output | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw "Unrelated native payload control passed: $control"}
    if (Test-Path -LiteralPath (Join-Path $output package-payload-comparison.json)) {throw 'A refused comparison published success evidence.'}
    if ((Get-FileHash -LiteralPath $invalid).Hash -cne $invalidHash) {throw 'Refusal modified a native control bundle.'}
}
$invalidContainer=Join-Path $fixture invalid.msixbundle
[IO.File]::WriteAllBytes($invalidContainer,[byte[]]@(0,1,2,3))
$invalidOutput=Join-Path $fixture invalid-container-comparison
$message=$null
try {& $test -FirstBundlePath $first -SecondBundlePath $invalidContainer -WorkingDirectory $invalidOutput | Out-Null} catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('MakeAppx refused the bundle',[StringComparison]::Ordinal)) {throw "Malformed container did not reach native refusal: $message"}
if (Test-Path -LiteralPath (Join-Path $invalidOutput package-payload-comparison.json)) {throw 'Native container refusal published success evidence.'}
if ((Get-FileHash -LiteralPath $first).Hash -cne $firstHash -or (Get-FileHash -LiteralPath $second).Hash -cne $secondHash) {throw 'Comparison changed an original native fixture container.'}
if ((Get-FileHash -LiteralPath $BundlePath).Hash -cne $originalHash) {throw 'Native fixture qualification changed its retained source bundle.'}
Write-Output "PASS: native validated identity-only bundles accepted truthfully; changed/extra/omitted ordinary files refused. Retained fixtures: $fixture"
