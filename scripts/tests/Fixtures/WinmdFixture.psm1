<# Native PE/metadata fixture construction shared by comparison tests.
   It supplies inputs only; no expected comparison decision is computed here. #>
function New-TestWinmd([string]$OutputDirectory,[string]$Name,[Guid]$ModuleVersionId,[uint32]$Timestamp,[string]$TypeName='FixtureInterface') {
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
    $path=Join-Path $OutputDirectory ($Name+'.winmd')
    $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew)
    try {$blob.WriteContentTo($stream)} finally {$stream.Dispose()}
    return $path
}
Export-ModuleMember -Function New-TestWinmd
