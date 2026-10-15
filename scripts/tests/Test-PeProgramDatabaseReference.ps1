<# Exercise actual native PE debug-directory records, not a token search in
   MSBuild XML. The good image must have a basename-only PDB reference; the
   retained pre-fix image provides the real absolute-path negative control. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PortableImagePath,
    [string]$AbsoluteReferenceImagePath = '',
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$validator = Join-Path $repositoryRoot 'scripts/Test-PeProgramDatabaseReference.ps1'

& $validator -ImagePath $PortableImagePath
if (-not [string]::IsNullOrEmpty($AbsoluteReferenceImagePath)) {
    $refusal = $null
    try { & $validator -ImagePath $AbsoluteReferenceImagePath | Out-Null }
    catch { $refusal = $_.Exception.Message }
    if ($null -eq $refusal -or -not $refusal.Contains('basename-only', [StringComparison]::Ordinal)) {
        throw "The native absolute PDB reference was not refused at the intended boundary: $refusal"
    }
    Write-Output 'PASS: native basename-only PDB reference accepted; absolute build-path reference refused.'
}

# Exercise refusal branches with Microsoft's PE writer. Do not byte-patch a
# good PE or reproduce the parser's binary-layout logic in its test oracle.
$fixtureRoot = Join-Path $OwnedOutputRoot ('pe-symbol-controls-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
function New-CodeViewImage([string]$Name, [string[]]$References) {
    $debugDirectory = [Reflection.PortableExecutable.DebugDirectoryBuilder]::new()
    $contentId = [Reflection.Metadata.BlobContentId]::new(
        [Guid]'8337d907-079c-4e91-a4f3-42802dbab783', [uint32]1)
    foreach ($reference in $References) { $debugDirectory.AddCodeViewEntry($reference, $contentId, [uint16]0) }
    $pe = [Reflection.PortableExecutable.ManagedPEBuilder]::new(
        [Reflection.PortableExecutable.PEHeaderBuilder]::new(),
        [Reflection.Metadata.Ecma335.MetadataRootBuilder]::new([Reflection.Metadata.Ecma335.MetadataBuilder]::new()),
        [Reflection.Metadata.BlobBuilder]::new(), $null, $null, $null, $debugDirectory, 0,
        [Reflection.Metadata.MethodDefinitionHandle]::new(), [Reflection.PortableExecutable.CorFlags]::ILOnly, $null)
    $blob = [Reflection.Metadata.BlobBuilder]::new()
    [void]$pe.Serialize($blob)
    $path = Join-Path $fixtureRoot ($Name + '.exe')
    $file = [IO.File]::Open($path, [IO.FileMode]::CreateNew)
    try { $blob.WriteContentTo($file) } finally { $file.Dispose() }
    return $path
}

& $validator -ImagePath (New-CodeViewImage 'valid-basename' @('fixture.pdb'))
$cases = @(
    @{name='drive-absolute';references=@('C:\private\fixture.pdb');diagnostic='basename-only'},
    @{name='unc-directory';references=@('\\host\private\fixture.pdb');diagnostic='basename-only'},
    @{name='drive-relative';references=@('C:fixture.pdb');diagnostic='basename-only'},
    @{name='relative-directory';references=@('symbols\fixture.pdb');diagnostic='basename-only'},
    @{name='posix-directory';references=@('/symbols/fixture.pdb');diagnostic='basename-only'},
    @{name='wrong-extension';references=@('fixture.dbg');diagnostic='basename-only'},
    @{name='missing-reference';references=@();diagnostic='exactly one'},
    @{name='ambiguous-references';references=@('one.pdb','two.pdb');diagnostic='exactly one'}
)
foreach ($case in $cases) {
    $image = New-CodeViewImage $case.name $case.references
    $diagnostic = $null
    try { & $validator -ImagePath $image | Out-Null }
    catch { $diagnostic = $_.Exception.Message }
    if ($null -eq $diagnostic -or -not $diagnostic.Contains($case.diagnostic, [StringComparison]::Ordinal)) {
        throw "PE symbol-reference control did not refuse $($case.name): $diagnostic"
    }
}
Write-Output 'PASS: native PE-writer controls reject directory-bearing, wrong-format, missing and ambiguous references.'
