<# Record an immutable extracted payload for cross-root reproducibility.
   Inputs must be quiescent, ordinary files; output is external and new. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PayloadRoot,
    [Parameter(Mandatory)][string]$OutputPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$canonicalRoot = [IO.Path]::GetFullPath($PayloadRoot)
$canonicalOutput = [IO.Path]::GetFullPath($OutputPath)
if (-not [IO.Directory]::Exists($canonicalRoot)) { throw 'PayloadRoot must be an existing extracted directory.' }
$outputRelativePath = [IO.Path]::GetRelativePath($canonicalRoot, $canonicalOutput)
if (-not [IO.Path]::IsPathRooted($outputRelativePath) -and
    $outputRelativePath -ne '..' -and -not $outputRelativePath.StartsWith("..$([IO.Path]::DirectorySeparatorChar)", [StringComparison]::Ordinal)) {
    throw 'Payload manifest output must be outside the payload root.'
}
if (Test-Path -LiteralPath $canonicalOutput) { throw 'Payload manifest output must be fresh and absent.' }

# Do not recurse through native enumeration before checking link attributes:
# a junction could otherwise inventory files outside the accepted payload.
$pendingDirectories = [Collections.Generic.Stack[string]]::new()
$pendingDirectories.Push($canonicalRoot)
$filesByRelativePath = [Collections.Generic.SortedDictionary[string,object]]::new([StringComparer]::Ordinal)
while ($pendingDirectories.Count -gt 0) {
    $directory = $pendingDirectories.Pop()
    if (([IO.File]::GetAttributes($directory) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Payload manifests refuse reparse points.'
    }
    foreach ($entry in [IO.Directory]::EnumerateFileSystemEntries($directory)) {
        $attributes = [IO.File]::GetAttributes($entry)
        if (($attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Payload manifests refuse reparse points.' }
        if (($attributes -band [IO.FileAttributes]::Directory) -ne 0) { $pendingDirectories.Push($entry); continue }
        $relativePath = [IO.Path]::GetRelativePath($canonicalRoot, $entry).Replace('\','/')
        if ([IO.Path]::IsPathRooted($relativePath) -or $relativePath.StartsWith('../',[StringComparison]::Ordinal)) {
            throw 'Payload file escaped the extracted root.'
        }
        if ($filesByRelativePath.ContainsKey($relativePath)) { throw 'Payload contains duplicate normalized relative paths.' }
        # OpenRead shares read access only on Windows, rejecting a concurrently
        # writable handle rather than hashing a deliberately mutable producer.
        # Caller-owned trees must remain quiescent for the entire observation.
        $input = [IO.File]::OpenRead($entry)
        try {
            $byteLength = $input.Length
            $sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($input)).ToLowerInvariant()
            if ($input.Length -ne $byteLength) { throw 'Payload file changed during hashing.' }
        }
        finally { $input.Dispose() }
        $filesByRelativePath.Add($relativePath, @{byteLength=$byteLength;sha256=$sha256})
    }
}

# Native JSON serialization owns escaping and UTF-8; fixed field/path ordering
# supplies the project-specific cross-root comparison contract, not a timestamp
# or machine-dependent absolute path. CreateNew never replaces an old result.
$output = [IO.FileStream]::new($canonicalOutput, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $writer = [Text.Json.Utf8JsonWriter]::new($output)
    try {
        $writer.WriteStartObject()
        $writer.WriteNumber('schemaVersion', [int]1)
        $writer.WriteStartArray('files')
        foreach ($entry in $filesByRelativePath.GetEnumerator()) {
            $writer.WriteStartObject()
            $writer.WriteString('relativePath', [string]$entry.Key)
            $writer.WriteNumber('byteLength', [long]$entry.Value.byteLength)
            $writer.WriteString('sha256', [string]$entry.Value.sha256)
            $writer.WriteEndObject()
        }
        $writer.WriteEndArray()
        $writer.WriteEndObject()
        $writer.Flush()
        $output.WriteByte(10)
        $output.Flush($true)
    }
    finally { $writer.Dispose() }
}
finally { $output.Dispose() }
Write-Output $canonicalOutput
