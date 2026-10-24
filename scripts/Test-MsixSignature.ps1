<# Verify an existing package/bundle, never sign or modify it. The caller owns
   a quiescent ordinary input and a fresh external diagnostic destination.
   This checks Windows signature trust, not JPG Spinner identity/installability. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$DiagnosticsPath
)
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1') -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$package = [IO.Path]::GetFullPath($PackagePath)
$diagnostics = [IO.Path]::GetFullPath($DiagnosticsPath)
if (-not [IO.File]::Exists($package) -or [IO.Path]::GetExtension($package) -cnotin @('.msix', '.msixbundle')) {
    throw 'Signature verification requires an existing .msix or .msixbundle.'
}
if (Test-Path -LiteralPath $diagnostics) { throw 'Signature diagnostics must be fresh and absent.' }
$tools = & (Join-Path $PSScriptRoot 'Resolve-MsixToolchain.ps1')
$handle = [IO.File]::OpenRead($package)
try {
    $sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($handle)).ToLowerInvariant()
    # Default Authentication Verification Policy, all embedded signatures; do
    # not accidentally use the default driver policy, catalog search, /p7-only
    # cryptographic checks or diagnostic-prose matching as a trust decision.
    # https://learn.microsoft.com/windows/win32/seccrypto/signtool
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $tools.SignToolExecutablePath
    $start.WorkingDirectory = Split-Path -Parent $PSScriptRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('verify', '/pa', '/all', $package)) { $start.ArgumentList.Add($argument) }
    $log = [IO.FileStream]::new($diagnostics, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'Native SignTool did not start.' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit((Get-QualificationTimeout -Category nativeValidation -Unit Milliseconds))) {
            $process.Kill($true)
            # Confirm termination only for this owned process; never use a
            # machine-wide process-name kill or retry trust verification.
            if (-not $process.WaitForExit((Get-QualificationTimeout -Category nativeTeardown -Unit Milliseconds))) { throw 'Native SignTool termination remains unresolved after its deadline.' }
            throw 'Native SignTool verification exceeded its qualification-policy deadline.'
        }
        $text = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        $bytes = [Text.Encoding]::UTF8.GetBytes($text)
        $log.Write($bytes)
        if ($process.ExitCode -ne 0) { throw 'Native SignTool refused the package signature.' }
    }
    finally { $process.Dispose(); $log.Dispose() }
}
finally { $handle.Dispose() }
[pscustomobject]@{PackagePath = $package; Sha256 = $sha256 }
