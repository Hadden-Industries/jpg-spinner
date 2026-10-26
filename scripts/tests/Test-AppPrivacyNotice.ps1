<# The complete application notice must remain available offline, and the
   published English copy must be generated from that same native content. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$shell = [Xml.XmlDocument]::new()
$shell.Load((Join-Path $repositoryRoot 'src/JpgSpinner.App/MainWindow.xaml'))
$ns = [Xml.XmlNamespaceManager]::new($shell.NameTable)
$ns.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
$ns.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
$notice = $shell.SelectSingleNode('//*[@x:Name="PrivacyNoticeContent"]', $ns)
if ($null -eq $notice) { throw 'The complete app privacy notice must be packaged locally.' }
$email = $notice.SelectNodes('.//ui:HyperlinkButton[@NavigateUri="mailto:info@haddenindustries.com"]', $ns)
if ($email.Count -ne 1) { throw 'Privacy contact must offer one native email link without a subject, body or attachment.' }
foreach ($heading in @('Shell_PrivacyProcessingHeading', 'Shell_PrivacyLocalDataHeading', 'Shell_PrivacyDiagnosticsHeading', 'Shell_PrivacyExportHeading', 'Shell_PrivacyExternalHeading')) {
    $section = $notice.SelectSingleNode(".//ui:Expander[ui:Expander.Header//*[@x:Uid='$heading']]", $ns)
    if ($null -eq $section) { throw "Keep $heading available in a native expandable offline section." }
}
foreach ($link in $notice.SelectNodes('.//ui:HyperlinkButton', $ns)) {
    if (([Uri]$link.GetAttribute('NavigateUri')).Host -ieq 'github.com') {
        throw 'The app notice must not send the reader to GitHub for its own privacy information.'
    }
}
foreach ($language in @('en-US', 'en-GB', 'ru')) {
    $resources = [Xml.XmlDocument]::new()
    $resources.Load((Join-Path $repositoryRoot "src/JpgSpinner.App/Strings/$language/Resources.resw"))
    foreach ($node in $notice.SelectNodes('.//*[@x:Uid]', $ns)) {
        $uid = $node.GetAttribute('Uid', 'http://schemas.microsoft.com/winfx/2006/xaml')
        $property = if ($node.LocalName -ceq 'HyperlinkButton') { 'Content' } else { 'Text' }
        $value = $resources.SelectSingleNode("/root/data[@name='$uid.$property']/value")
        if ($null -eq $value -or [string]::IsNullOrWhiteSpace($value.InnerText)) {
            throw "Incomplete $language privacy notice: $uid.$property"
        }
    }
}
& (Join-Path $repositoryRoot 'scripts/Export-AppPrivacyNotice.ps1') -Check
Write-Output 'PASS: complete offline localized notice and identical published English content.'
