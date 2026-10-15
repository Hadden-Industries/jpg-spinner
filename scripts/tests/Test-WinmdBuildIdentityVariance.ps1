<# Native metadata/PE writers supply independent identity-only and type-change
   fixtures. This tests a read-only comparison, never artifact rewriting. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test=Join-Path $root 'scripts/Test-WinmdBuildIdentityVariance.ps1'
$fixture=Join-Path $OwnedOutputRoot ('winmd-identity-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Import-Module (Join-Path $PSScriptRoot 'Fixtures/WinmdFixture.psm1') -Force
$first=New-TestWinmd $fixture 'first' ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 1
$second=New-TestWinmd $fixture 'identity-only' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2
$before=@{First=(Get-FileHash $first).Hash;Second=(Get-FileHash $second).Hash}
$same=& $test -FirstWinmdPath $first -SecondWinmdPath $first
if (-not $same.ByteIdentical) {throw 'Identical native metadata was not accepted as byte-identical.'}
$different=& $test -FirstWinmdPath $first -SecondWinmdPath $second
if ($different.ByteIdentical -or -not $different.IdentityVarianceObserved -or
    $different.FirstTimestamp -ne 1 -or $different.SecondTimestamp -ne 2 -or
    $different.FirstMvid -ne [Guid]'384e147e-1a1d-46c4-837e-25af7c907767' -or
    $different.SecondMvid -ne [Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') {throw 'Native identity variance was concealed or not accepted accurately.'}
foreach ($identityOnly in @(
    (New-TestWinmd $fixture 'timestamp-only' ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 2),
    (New-TestWinmd $fixture 'mvid-only' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 1)
)) {
    $result=& $test -FirstWinmdPath $first -SecondWinmdPath $identityOnly
    if ($result.ByteIdentical -or -not $result.IdentityVarianceObserved) {throw 'An isolated native identity field was not compared accurately.'}
}
$changedType=New-TestWinmd $fixture 'changed-type' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2 'ChangedInterface'
if ((Get-Item -LiteralPath $changedType).Length -ne (Get-Item -LiteralPath $first).Length) {throw 'Type-change control must reach byte comparison, not length refusal.'}
$message=$null
try {& $test -FirstWinmdPath $first -SecondWinmdPath $changedType | Out-Null} catch {$message=$_.Exception.Message}
if ($null -eq $message) {throw 'Changed metadata type content passed the identity-only policy.'}
$missingIdentity=New-TestWinmd $fixture 'missing-mvid' ([Guid]::Empty) 1
$invalidImage=Join-Path $fixture invalid.winmd
[IO.File]::WriteAllBytes($invalidImage,[byte[]]@(0,1,2,3))
foreach ($invalid in @($missingIdentity,$invalidImage)) {
    $message=$null
    try {& $test -FirstWinmdPath $first -SecondWinmdPath $invalid | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw 'Missing identity or malformed native metadata passed comparison.'}
}
if ((Get-FileHash $first).Hash -cne $before.First -or (Get-FileHash $second).Hash -cne $before.Second) {throw 'Comparison changed an original artifact.'}
Write-Output 'PASS: identical/identity-only native metadata accepted honestly; type changes refused; originals unchanged.'
