<# A release image must not record a developer's absolute PDB location.
   Let .NET's PEReader interpret the PE/CodeView formats; do not parse DUMPBIN
   text or implement another binary reader. This is a symbol-reference/privacy
   gate, not proof of deterministic code generation or matching PDB contents.
   https://learn.microsoft.com/cpp/build/reference/pdbaltpath-use-alternate-pdb-path
   https://learn.microsoft.com/dotnet/api/system.reflection.portableexecutable.pereader.readcodeviewdebugdirectorydata #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ImagePath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$stream = [IO.File]::OpenRead([IO.Path]::GetFullPath($ImagePath))
try {
    $reader = [Reflection.PortableExecutable.PEReader]::new($stream)
    try {
        $codeViewEntries = @($reader.ReadDebugDirectory() | Where-Object {
            $_.Type -eq [Reflection.PortableExecutable.DebugDirectoryEntryType]::CodeView
        })
        # The release contract retains symbols. An absent/ambiguous reference
        # must not pass merely because there is no pathname left to inspect.
        if ($codeViewEntries.Count -ne 1) {
            throw 'Release PE must have exactly one CodeView program-database reference.'
        }
        $reference = $reader.ReadCodeViewDebugDirectoryData($codeViewEntries[0])
        $fileName = $reference.Path
        if ([string]::IsNullOrWhiteSpace($fileName) -or
            $fileName.IndexOfAny([char[]]"\/:`r`n") -ge 0 -or
            $fileName -in @('.', '..') -or
            -not $fileName.EndsWith('.pdb', [StringComparison]::OrdinalIgnoreCase)) {
            # Do not repeat the rejected path in public diagnostics: it may
            # contain a private workstation username or checkout directory.
            throw 'Release PE requires a basename-only .pdb reference; build paths are not distributable.'
        }
    }
    finally { $reader.Dispose() }
}
finally { $stream.Dispose() }
Write-Output 'PASS: release PE carries a basename-only program-database reference.'
