<# Exercise the native PE reader against real executable images. This checks
   identity/refusal behavior without launching either fixture executable. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Arm64ImagePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$validator = Join-Path (Split-Path -Parent $PSScriptRoot) 'Test-ExecutableArchitecture.ps1'
$nativeImage = (Get-Process -Id $PID).Path
$nativeArchitecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
$observed = & $validator -ImagePath $nativeImage -Architecture $nativeArchitecture -RequireNativeHost
if ($observed.imageSha256 -cne (Get-FileHash -LiteralPath $nativeImage).Hash.ToLowerInvariant()) {
    throw 'Native executable identity lost its actual digest.'
}
$arm = & $validator -ImagePath $Arm64ImagePath -Architecture ARM64
if ($arm.machine -cne 'Arm64') { throw 'The actual Microsoft ARM64 image was not identified natively.' }
function Assert-Refused([scriptblock]$Action, [string]$Diagnostic) {
    $message = $null
    try { & $Action | Out-Null } catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($Diagnostic, [StringComparison]::Ordinal)) {
        throw "Expected architecture refusal '$Diagnostic', observed '$message'."
    }
}
$fixture = Join-Path $OwnedOutputRoot ('executable-architecture-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$invalid = Join-Path $fixture 'not-an-image.exe'
[IO.File]::WriteAllText($invalid, 'Intentionally not a PE image; never executed.')
Assert-Refused { & $validator -ImagePath $Arm64ImagePath -Architecture x64 } 'Executable machine differs'
Assert-Refused { & $validator -ImagePath $invalid -Architecture x64 } 'Invalid PE image'
Assert-Refused { & $validator -ImagePath (Join-Path ([Environment]::SystemDirectory) 'kernel32.dll') -Architecture $nativeArchitecture } 'Native executable PE image is required'
Assert-Refused { & $validator -ImagePath (Join-Path $fixture 'absent.exe') -Architecture ARM64 } 'Could not find'
if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne [Runtime.InteropServices.Architecture]::Arm64) {
    Assert-Refused { & $validator -ImagePath $Arm64ImagePath -Architecture ARM64 -RequireNativeHost } 'Native OS architecture differs'
    # Exercise the public suite runner's admission, not just its reader helper.
    # An owned non-executable sentinel occupies the expected test locator:
    # the nonnative OS must be refused before PE reading or Process.Start.
    $repository = Join-Path $fixture 'deployment'
    $project = 'JpgSpinner.Domain.Tests'
    $binaryRoot = Join-Path $repository "artifacts/bin/ARM64/Release/$project"
    [void][IO.Directory]::CreateDirectory($binaryRoot)
    [IO.File]::WriteAllText((Join-Path $binaryRoot "$project.exe"), 'Owned non-executable admission sentinel.')
    $suite = Join-Path (Split-Path -Parent $PSScriptRoot) 'Invoke-TestSuite.ps1'
    Assert-Refused { & $suite -Project $project -Configuration Release -Architecture ARM64 -RepositoryRoot $repository -RequireNativeArchitecture } 'Native OS architecture differs'
    if (Test-Path -LiteralPath (Join-Path $repository "artifacts/TestResults/ARM64/Release/$project.junit.xml")) {
        throw 'Refused native admission reached test-report production.'
    }
}
Write-Output 'PASS: actual native/ARM64 PE identities, hash binding, invalid/missing/wrong-machine and nonnative-host refusals; no fixture execution.'
