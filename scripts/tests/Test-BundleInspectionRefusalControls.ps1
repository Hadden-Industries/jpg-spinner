<# Real filesystem controls refuse before native extraction; no source bundle
   mutation, signing, installation, or trust-store changes are permitted. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer = Join-Path $repositoryRoot 'scripts/Expand-JpgSpinnerBundle.ps1'
if (-not [IO.File]::Exists($BundlePath)) { throw 'The real bundle fixture is absent.' }
$before = (Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash
$fixture = Join-Path $OwnedOutputRoot ('bundle-refusal-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$ordinary = Join-Path $fixture 'ordinary'
[void][IO.Directory]::CreateDirectory($ordinary)
$junction = Join-Path $fixture 'redirected'
[void](New-Item -ItemType Junction -Path $junction -Target $ordinary)
if (([IO.File]::GetAttributes($junction) -band [IO.FileAttributes]::ReparsePoint) -eq 0) {
    throw 'Actual reparse-point fixture precondition is absent.'
}
foreach ($case in @(
    @{Output=$fixture;Diagnostic='fresh absent directory'},
    @{Output=(Join-Path $junction 'must-not-exist');Diagnostic='reparse-point'}
)) {
    $message = $null
    try { & $producer -BundlePath $BundlePath -InspectionDirectory $case.Output | Out-Null }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal)) {
        throw "Expected inspection refusal '$($case.Diagnostic)', received '$message'."
    }
}
if ((Test-Path -LiteralPath (Join-Path $ordinary 'must-not-exist')) -or
    @(Get-ChildItem -LiteralPath $fixture -File).Count -ne 0 -or
    (Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash -cne $before) {
    throw 'Inspection refusal changed the bundle or created extraction outputs.'
}

# A truncated copy exercises the actual SDK reader's refusal, not a mocked
# process exit code. Only this UUID-owned generated copy is truncated.
$truncated = Join-Path $fixture 'truncated.msixbundle'
Copy-Item -LiteralPath $BundlePath -Destination $truncated
$stream = [IO.File]::Open($truncated,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $stream.SetLength(1024) } finally { $stream.Dispose() }
if ((Get-Item -LiteralPath $truncated).Length -ne 1024) { throw 'Truncated container precondition is absent.' }
$nativeOutput = Join-Path $fixture 'native-refusal'
$message = $null
$successValues = @()
try { $successValues = @(& $producer -BundlePath $truncated -InspectionDirectory $nativeOutput) }
catch { $message = $_.Exception.Message }
if ($message -cne 'Native MakeAppx refused the bundle.' -or $successValues.Count -ne 0 -or
    (Test-Path -LiteralPath (Join-Path $nativeOutput 'payloads')) -or
    -not [IO.File]::Exists((Join-Path $nativeOutput 'diagnostics/unbundle.log')) -or
    (Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash -cne $before) {
    throw "Native truncated-bundle refusal was absent or promoted partial output: $message"
}
Write-Output 'PASS: existing output, actual ancestor junction and truncated native bundle refuse without input mutation or promotion.'
