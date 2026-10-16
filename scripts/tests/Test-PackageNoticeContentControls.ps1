<# Exercise realistic notice-content mistakes using a fresh copy of the owning
   MSBuild result. After a native package run, also qualify missing/edited payload
   refusals on a small owned copy; never modify the actual package or sources. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ContentResultPath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [string]$PackageDirectory=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$contract = Join-Path $PSScriptRoot 'Test-PackageNoticeContent.ps1'
$fixture = Join-Path $OwnedOutputRoot ('package-notice-controls-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
& $contract -ContentResultPath $ContentResultPath
$baseline = Get-Content -LiteralPath $ContentResultPath -Raw

function Assert-ContentRefusal {
    <# Persist one modified native-result fixture and require the exact contract
       diagnostic, so unrelated parser/setup failures cannot satisfy a control. #>
    param([Parameter(Mandatory)]$Result,[Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$ExpectedMessage)
    $path = Join-Path $fixture "$Name.json"
    [IO.File]::WriteAllText($path,($Result | ConvertTo-Json -Depth 100))
    $message=$null
    try { & $contract -ContentResultPath $path } catch { $message=$_.Exception.Message }
    if ($message -cne $ExpectedMessage) { throw "Control $Name did not reach its intended refusal: $message" }
}

$result = $baseline | ConvertFrom-Json -Depth 100
$result.TargetResults.ContentFilesProjectOutputGroup.Items = @(
    $result.TargetResults.ContentFilesProjectOutputGroup.Items | Where-Object TargetPath -CNE 'LICENSE.txt'
)
Assert-ContentRefusal -Result $result -Name 'missing-application-license' `
    -ExpectedMessage 'MSBuild content group requires exactly one notice: LICENSE.txt'

$result = $baseline | ConvertFrom-Json -Depth 100
$licenseItem = @($result.TargetResults.ContentFilesProjectOutputGroup.Items | Where-Object TargetPath -CEQ 'LICENSE.txt')[0]
$result.TargetResults.ContentFilesProjectOutputGroup.Items += $licenseItem
Assert-ContentRefusal -Result $result -Name 'duplicate-target' `
    -ExpectedMessage 'MSBuild content group requires exactly one notice: LICENSE.txt'

$result = $baseline | ConvertFrom-Json -Depth 100
$ijgItem = @($result.TargetResults.ContentFilesProjectOutputGroup.Items | Where-Object TargetPath -CEQ 'Licenses\native\libjpeg-turbo.README.ijg.txt')[0]
$ijgItem.FullPath = Join-Path $root 'docs/licenses/native/libjpeg-turbo.copyright.txt'
Assert-ContentRefusal -Result $result -Name 'wrong-ijg-source' `
    -ExpectedMessage 'MSBuild notice source differs: Licenses/native/libjpeg-turbo.README.ijg.txt'

$result = $baseline | ConvertFrom-Json -Depth 100
# A wildcard over all retained notices would silently include this test-only
# input. Use the real source file to distinguish that wiring defect from absence.
$catchNotice = Join-Path $root 'docs/licenses/native/catch2.copyright.txt'
if (-not (Test-Path -LiteralPath $catchNotice -PathType Leaf)) { throw 'Test-only notice control input is absent.' }
$result.TargetResults.ContentFilesProjectOutputGroup.Items += [pscustomobject]@{
    TargetPath='Licenses/native/catch2.copyright.txt'; FullPath=$catchNotice
}
Assert-ContentRefusal -Result $result -Name 'test-only-notice' `
    -ExpectedMessage 'Unexpected package notice content: Licenses/native/catch2.copyright.txt'

if (-not [string]::IsNullOrEmpty($PackageDirectory)) {
    & $contract -PackageDirectory $PackageDirectory
    $payload = Join-Path $fixture 'payload'
    [void][IO.Directory]::CreateDirectory($payload)
    foreach ($relative in @('AppxManifest.xml','LICENSE.txt','LicenseNotices.txt','Licenses')) {
        Copy-Item -LiteralPath (Join-Path $PackageDirectory $relative) -Destination (Join-Path $payload $relative) -Recurse
    }
    & $contract -PackageDirectory $payload
    $ijgPath = Join-Path $payload 'Licenses/native/libjpeg-turbo.README.ijg.txt'
    Move-Item -LiteralPath $ijgPath -Destination "$ijgPath.held"
    $message=$null
    try { & $contract -PackageDirectory $payload } catch { $message=$_.Exception.Message }
    if ($message -cne 'Package notice is absent: Licenses/native/libjpeg-turbo.README.ijg.txt') {
        throw "Missing-package-notice control did not reach its intended refusal: $message"
    }
    Move-Item -LiteralPath "$ijgPath.held" -Destination $ijgPath
    [IO.File]::WriteAllText($ijgPath,'A shortened label is not the required IJG notice.')
    $message=$null
    try { & $contract -PackageDirectory $payload } catch { $message=$_.Exception.Message }
    if ($message -cne 'Package notice bytes differ: Licenses/native/libjpeg-turbo.README.ijg.txt') {
        throw "Edited-package-notice control did not reach its intended refusal: $message"
    }
    Write-Output 'PASS: native-package baseline and missing/edited notice controls.'
}
Write-Output 'PASS: missing application license, duplicate target, wrong IJG source and test-only notice controls.'
