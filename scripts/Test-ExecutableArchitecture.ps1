<# Inspect actual PE machine identities through .NET's maintained reader.
   Optional native-host admission excludes Windows' x86/x64 emulation on ARM64.
   This reads bytes and hashes only; successful test execution is separate proof.
   https://learn.microsoft.com/dotnet/api/system.reflection.portableexecutable.pereader
   https://learn.microsoft.com/dotnet/api/system.runtime.interopservices.runtimeinformation.osarchitecture #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$ImagePath,
    [Parameter(Mandatory)][ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture,
    [switch]$RequireNativeHost
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$osArchitecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture
$machineByArchitecture = @{
    x86   = [Reflection.PortableExecutable.Machine]::I386
    x64   = [Reflection.PortableExecutable.Machine]::Amd64
    ARM64 = [Reflection.PortableExecutable.Machine]::Arm64
}
if ($RequireNativeHost -and -not [string]::Equals(
        $osArchitecture.ToString(), $Architecture, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Native OS architecture differs from the selected test architecture.'
}
foreach ($path in $ImagePath) {
    $stream = [IO.File]::OpenRead([IO.Path]::GetFullPath($path))
    try {
        $reader = [Reflection.PortableExecutable.PEReader]::new($stream)
        try {
            try { $machine = $reader.PEHeaders.CoffHeader.Machine }
            catch { throw 'Invalid PE image; executable architecture cannot be established.' }
            $characteristics = $reader.PEHeaders.CoffHeader.Characteristics
            if ($null -eq $reader.PEHeaders.PEHeader -or $reader.HasMetadata -or
                ($characteristics -band [Reflection.PortableExecutable.Characteristics]::ExecutableImage) -eq 0 -or
                ($characteristics -band [Reflection.PortableExecutable.Characteristics]::Dll) -ne 0) {
                # These C++ tests must be native images, not COFF objects or
                # DLLs or managed AnyCPU assemblies whose machine is insufficient.
                throw 'Native executable PE image is required.'
            }
            if ($machine -ne $machineByArchitecture[$Architecture]) {
                throw 'Executable machine differs from the selected test architecture.'
            }
            # Hash the same opened file inspected above; do not reopen a pathname
            # and accidentally report another file's digest after replacement.
            $stream.Position = 0
            $hash = [Security.Cryptography.SHA256]::HashData($stream)
            [pscustomobject]@{
                imageName            = [IO.Path]::GetFileName($path)
                imageSha256          = [Convert]::ToHexString($hash).ToLowerInvariant()
                machine              = $machine.ToString()
                osArchitecture       = $osArchitecture.ToString()
                observerArchitecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
                osDescription        = [Runtime.InteropServices.RuntimeInformation]::OSDescription
            }
        }
        finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
}
