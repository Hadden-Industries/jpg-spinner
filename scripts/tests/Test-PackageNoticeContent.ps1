<# Verify the application's notice-content contract at either the owning MSBuild
   output group or an actual SDK-unpacked package. This checks paths and exact
   byte preservation, not component contribution, legal clearance or source delivery.
   ContentResultPath is MSBuild's native -getTargetResult output, not project XML. #>
[CmdletBinding(DefaultParameterSetName='ContentGroup')]
param(
    [Parameter(Mandatory,ParameterSetName='ContentGroup')][string]$ContentResultPath,
    [Parameter(Mandatory,ParameterSetName='Package')][string]$PackageDirectory,
    [string]$RepositoryRoot=(Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

# This independent expected set deliberately excludes test-only Catch2, inventory
# READMEs and engineering-tool notices. Conservative inclusion of build-context
# inputs is not an assertion that every input contributes runtime code.
$expectedSources = [ordered]@{
    'LICENSE.txt'='LICENSE'
    'LicenseNotices.txt'='src/JpgSpinner.App/LicenseNotices.txt'
    'Licenses/native/adobe-xmp-core.copyright.txt'='docs/licenses/native/adobe-xmp-core.copyright.txt'
    'Licenses/native/exiv2.copyright.txt'='docs/licenses/native/exiv2.copyright.txt'
    'Licenses/native/expat.copyright.txt'='docs/licenses/native/expat.copyright.txt'
    'Licenses/native/inih.copyright.txt'='docs/licenses/native/inih.copyright.txt'
    'Licenses/native/libjpeg-turbo.copyright.txt'='docs/licenses/native/libjpeg-turbo.copyright.txt'
    'Licenses/native/libjpeg-turbo.README.ijg.txt'='docs/licenses/native/libjpeg-turbo.README.ijg.txt'
    'Licenses/native/libspng.copyright.txt'='docs/licenses/native/libspng.copyright.txt'
    'Licenses/native/zlib.copyright.txt'='docs/licenses/native/zlib.copyright.txt'
    'Licenses/Microsoft.Windows.ImplementationLibrary.LICENSE.txt'='docs/licenses/Microsoft.Windows.ImplementationLibrary.LICENSE.txt'
    'Licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt'='docs/licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt'
    'Licenses/nuget/Microsoft.Web.WebView2.NOTICE.txt'='docs/licenses/nuget/Microsoft.Web.WebView2.NOTICE.txt'
    'Licenses/nuget/Microsoft.Windows.AI.MachineLearning.LICENSE.txt'='docs/licenses/nuget/Microsoft.Windows.AI.MachineLearning.LICENSE.txt'
    'Licenses/nuget/Microsoft.Windows.AI.MachineLearning.ThirdPartyNotices.txt'='docs/licenses/nuget/Microsoft.Windows.AI.MachineLearning.ThirdPartyNotices.txt'
    'Licenses/nuget/Microsoft.Windows.CppWinRT.LICENSE.txt'='docs/licenses/nuget/Microsoft.Windows.CppWinRT.LICENSE.txt'
    'Licenses/nuget/Microsoft.Windows.ImplementationLibrary.ThirdPartyNotices.txt'='docs/licenses/nuget/Microsoft.Windows.ImplementationLibrary.ThirdPartyNotices.txt'
    'Licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.LICENSE.txt'='docs/licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.LICENSE.txt'
    'Licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.NOTICE.txt'='docs/licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.NOTICE.txt'
    'Licenses/nuget/Microsoft.WindowsAppSDK.LICENSE.txt'='docs/licenses/nuget/Microsoft.WindowsAppSDK.LICENSE.txt'
    'Licenses/nuget/Microsoft.WindowsAppSDK.NOTICE.txt'='docs/licenses/nuget/Microsoft.WindowsAppSDK.NOTICE.txt'
    'Licenses/nuget/Microsoft.WindowsAppSDK.ML.LICENSE.txt'='docs/licenses/nuget/Microsoft.WindowsAppSDK.ML.LICENSE.txt'
    'Licenses/nuget/Microsoft.WindowsAppSDK.WinUI.NOTICE.txt'='docs/licenses/nuget/Microsoft.WindowsAppSDK.WinUI.NOTICE.txt'
    'Licenses/nuget/Microsoft.WindowsAppSDK.WinUI.Tools.NOTICE.txt'='docs/licenses/nuget/Microsoft.WindowsAppSDK.WinUI.Tools.NOTICE.txt'
}
$contentItems = @()
if ($PSCmdlet.ParameterSetName -eq 'ContentGroup') {
    Import-Module (Join-Path $RepositoryRoot 'scripts/JsonObjectMemberValidation.psm1') -Force
    $result = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $ContentResultPath -Raw) `
        -SourceDescription 'MSBuild content output group' -RequiredRootValueKind Object -MaximumDepth 100
    if ($result.TargetResults.ContentFilesProjectOutputGroup.Result -cne 'Success') {
        throw 'The owning MSBuild content output group did not succeed.'
    }
    $contentItems = @($result.TargetResults.ContentFilesProjectOutputGroup.Items)
}
elseif (-not (Test-Path -LiteralPath (Join-Path $PackageDirectory 'AppxManifest.xml') -PathType Leaf)) {
    throw 'Package notice qualification requires an actual unpacked application manifest.'
}
foreach ($entry in $expectedSources.GetEnumerator()) {
    $source = Join-Path $RepositoryRoot $entry.Value
    if ($PSCmdlet.ParameterSetName -eq 'ContentGroup') {
        $matches = @($contentItems | Where-Object { $_.TargetPath.Replace('\','/') -ceq $entry.Key })
        if ($matches.Count -ne 1) { throw "MSBuild content group requires exactly one notice: $($entry.Key)" }
        if (-not [IO.Path]::GetFullPath($source).Equals([IO.Path]::GetFullPath($matches[0].FullPath),[StringComparison]::OrdinalIgnoreCase)) {
            throw "MSBuild notice source differs: $($entry.Key)"
        }
        $actual = $matches[0].FullPath
    }
    else { $actual = Join-Path $PackageDirectory $entry.Key }
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Notice source is absent: $($entry.Value)" }
    if (-not (Test-Path -LiteralPath $actual -PathType Leaf)) { throw "Package notice is absent: $($entry.Key)" }
    # Hash the original bytes, not decoded/re-encoded text: vendor BOMs, endings
    # and final-newline absence are part of the retained notice contract.
    if ((Get-FileHash -LiteralPath $actual -Algorithm SHA256).Hash -cne
        (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash) {
        throw "Package notice bytes differ: $($entry.Key)"
    }
}
$actualNoticePaths = if ($PSCmdlet.ParameterSetName -eq 'ContentGroup') {
    @($contentItems | ForEach-Object { $_.TargetPath.Replace('\','/') } |
        Where-Object { $_.StartsWith('Licenses/',[StringComparison]::OrdinalIgnoreCase) })
}
else {
    @(Get-ChildItem -LiteralPath (Join-Path $PackageDirectory 'Licenses') -File -Recurse |
        ForEach-Object { [IO.Path]::GetRelativePath($PackageDirectory,$_.FullName).Replace('\','/') })
}
foreach ($path in $actualNoticePaths) {
    if (-not $expectedSources.Contains($path)) { throw "Unexpected package notice content: $path" }
}
Write-Output "PASS: $($expectedSources.Count) exact application/build-input notice assets ($($PSCmdlet.ParameterSetName)); no legal or complete component-coverage claim."
