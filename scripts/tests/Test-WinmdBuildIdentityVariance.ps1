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
function New-WinmdFixture([string]$Name,[Guid]$ModuleVersionId,[uint32]$Timestamp,[string]$TypeName='FixtureInterface') {
    # Native builders own every layout, handle, heap and PE header in the test.
    $metadata=[Reflection.Metadata.Ecma335.MetadataBuilder]::new()
    [void]$metadata.AddModule(0,$metadata.GetOrAddString('Fixture.winmd'),$metadata.GetOrAddGuid($ModuleVersionId),
        [Reflection.Metadata.GuidHandle]::new(),[Reflection.Metadata.GuidHandle]::new())
    [void]$metadata.AddAssembly($metadata.GetOrAddString('Fixture'),[Version]'1.0.0.0',
        [Reflection.Metadata.StringHandle]::new(),[Reflection.Metadata.BlobHandle]::new(),
        [Reflection.AssemblyFlags]::WindowsRuntime,[Reflection.AssemblyHashAlgorithm]::None)
    [void]$metadata.AddTypeDefinition(
        ([Reflection.TypeAttributes]::Public -bor [Reflection.TypeAttributes]::Interface -bor [Reflection.TypeAttributes]::Abstract -bor [Reflection.TypeAttributes]::WindowsRuntime),
        $metadata.GetOrAddString('Fixture'),$metadata.GetOrAddString($TypeName),
        [Reflection.Metadata.EntityHandle]::new(),
        [Reflection.Metadata.Ecma335.MetadataTokens]::FieldDefinitionHandle(1),
        [Reflection.Metadata.Ecma335.MetadataTokens]::MethodDefinitionHandle(1))
    $contentId=[Reflection.Metadata.BlobContentId]::new([Guid]'63d0cb48-2c67-4341-800b-5e8850420ab8',$Timestamp)
    $provider=[Func[Collections.Generic.IEnumerable[Reflection.Metadata.Blob],Reflection.Metadata.BlobContentId]]{param($blobs) $contentId}
    $builder=[Reflection.PortableExecutable.ManagedPEBuilder]::new(
        [Reflection.PortableExecutable.PEHeaderBuilder]::new(),
        [Reflection.Metadata.Ecma335.MetadataRootBuilder]::new($metadata,'WindowsRuntime 1.4'),
        [Reflection.Metadata.BlobBuilder]::new(),$null,$null,$null,$null,0,
        [Reflection.Metadata.MethodDefinitionHandle]::new(),[Reflection.PortableExecutable.CorFlags]::ILOnly,$provider)
    $blob=[Reflection.Metadata.BlobBuilder]::new()
    [void]$builder.Serialize($blob)
    $path=Join-Path $fixture ($Name+'.winmd')
    $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew)
    try {$blob.WriteContentTo($stream)} finally {$stream.Dispose()}
    return $path
}
$first=New-WinmdFixture 'first' ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 1
$second=New-WinmdFixture 'identity-only' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2
$before=@{First=(Get-FileHash $first).Hash;Second=(Get-FileHash $second).Hash}
$same=& $test -FirstWinmdPath $first -SecondWinmdPath $first
if (-not $same.ByteIdentical) {throw 'Identical native metadata was not accepted as byte-identical.'}
$different=& $test -FirstWinmdPath $first -SecondWinmdPath $second
if ($different.ByteIdentical -or -not $different.IdentityVarianceObserved -or
    $different.FirstTimestamp -ne 1 -or $different.SecondTimestamp -ne 2 -or
    $different.FirstMvid -ne [Guid]'384e147e-1a1d-46c4-837e-25af7c907767' -or
    $different.SecondMvid -ne [Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') {throw 'Native identity variance was concealed or not accepted accurately.'}
foreach ($identityOnly in @(
    (New-WinmdFixture 'timestamp-only' ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 2),
    (New-WinmdFixture 'mvid-only' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 1)
)) {
    $result=& $test -FirstWinmdPath $first -SecondWinmdPath $identityOnly
    if ($result.ByteIdentical -or -not $result.IdentityVarianceObserved) {throw 'An isolated native identity field was not compared accurately.'}
}
$changedType=New-WinmdFixture 'changed-type' ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2 'ChangedInterface'
if ((Get-Item -LiteralPath $changedType).Length -ne (Get-Item -LiteralPath $first).Length) {throw 'Type-change control must reach byte comparison, not length refusal.'}
$message=$null
try {& $test -FirstWinmdPath $first -SecondWinmdPath $changedType | Out-Null} catch {$message=$_.Exception.Message}
if ($null -eq $message) {throw 'Changed metadata type content passed the identity-only policy.'}
$missingIdentity=New-WinmdFixture 'missing-mvid' ([Guid]::Empty) 1
$invalidImage=Join-Path $fixture invalid.winmd
[IO.File]::WriteAllBytes($invalidImage,[byte[]]@(0,1,2,3))
foreach ($invalid in @($missingIdentity,$invalidImage)) {
    $message=$null
    try {& $test -FirstWinmdPath $first -SecondWinmdPath $invalid | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw 'Missing identity or malformed native metadata passed comparison.'}
}
if ((Get-FileHash $first).Hash -cne $before.First -or (Get-FileHash $second).Hash -cne $before.Second) {throw 'Comparison changed an original artifact.'}
Write-Output 'PASS: identical/identity-only native metadata accepted honestly; type changes refused; originals unchanged.'
