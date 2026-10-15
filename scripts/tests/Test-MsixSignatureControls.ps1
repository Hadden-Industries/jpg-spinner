<# Use a real Microsoft-signed runtime package and an unsigned development
   bundle. Tampering affects only a generated copy; originals stay unchanged. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SignedPackagePath,
    [Parameter(Mandatory)][string]$UnsignedBundlePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$validator = Join-Path $root 'scripts/Test-MsixSignature.ps1'
$fixture = Join-Path $OwnedOutputRoot ('msix-signature-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$signedHash = (Get-FileHash -LiteralPath $SignedPackagePath -Algorithm SHA256).Hash.ToLowerInvariant()
$unsignedHash = (Get-FileHash -LiteralPath $UnsignedBundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
$message = $null
$accepted = @()
try { $accepted = @(& $validator -PackagePath $UnsignedBundlePath -DiagnosticsPath (Join-Path $fixture 'unsigned.log')) }
catch { $message = $_.Exception.Message }
if ($message -cne 'Native SignTool refused the package signature.' -or $accepted.Count -ne 0) {
    throw "Unsigned bundle was not refused by native trust verification: $message"
}
$result = & $validator -PackagePath $SignedPackagePath -DiagnosticsPath (Join-Path $fixture 'signed.log')
if ($result.Sha256 -cne $signedHash) { throw 'Successful native signature validation did not identify the exact container bytes.' }

$tampered = Join-Path $fixture 'tampered.msix'
Copy-Item -LiteralPath $SignedPackagePath -Destination $tampered
# Standard ZIP APIs construct a well-formed but signature-invalid fixture. This
# is test-only mutation, not a production package reader or signature parser.
$archive = [IO.Compression.ZipFile]::Open($tampered,[IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.CreateEntry('signature-negative-control.txt')
    $stream = $entry.Open()
    try { $stream.WriteByte(42) } finally { $stream.Dispose() }
}
finally { $archive.Dispose() }
$archive = [IO.Compression.ZipFile]::OpenRead($tampered)
try {
    if ($null -eq $archive.GetEntry('AppxSignature.p7x') -or $null -eq $archive.GetEntry('signature-negative-control.txt')) {
        throw 'Well-formed signed-container tampering precondition is absent.'
    }
}
finally { $archive.Dispose() }
$message = $null
try { & $validator -PackagePath $tampered -DiagnosticsPath (Join-Path $fixture 'tampered.log') | Out-Null }
catch { $message = $_.Exception.Message }
if ($message -cne 'Native SignTool refused the package signature.') { throw "Tampered signed package was not refused: $message" }
if ((Get-FileHash -LiteralPath $SignedPackagePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $signedHash -or
    (Get-FileHash -LiteralPath $UnsignedBundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $unsignedHash) {
    throw 'Signature qualification changed an original input.'
}
Write-Output 'PASS: native SDK verifies a genuine signed MSIX and rejects unsigned/tampered containers without changing originals.'
