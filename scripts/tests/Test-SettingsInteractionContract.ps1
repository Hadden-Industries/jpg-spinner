<# Regression guard for native interaction wiring. Runtime transaction tests
   separately prove that replacing originals does not retain a backup. #>
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
foreach ($view in @('CropExplanationView', 'ProgressiveExplanationView')) {
    foreach ($text in $shell.SelectNodes("//*[@x:Name='$view']//ui:TextBlock", $ns)) {
        if ($text.GetAttribute('IsTextSelectionEnabled') -cne 'True') {
            throw "Help text must support native selection and copying: $($text.OuterXml)"
        }
    }
}
foreach ($header in $shell.SelectNodes('//ui:Expander.Header', $ns)) {
    if ($header.FirstChild.GetAttribute('IsHitTestVisible') -cne 'False') {
        throw 'Decorative disclosure headers must leave input to the native Expander toggle.'
    }
}
$support = $shell.SelectSingleNode('//ui:Grid[@x:Name="SettingsSupportCards"]', $ns)
if ($null -eq $support -or $support.SelectNodes('ui:Border', $ns).Count -ne 2) {
    throw 'Diagnostics and About must share one responsive support grid.'
}
$wide = $support.SelectSingleNode('.//ui:VisualState[@x:Name="SettingsSupportWide"]', $ns)
if ($null -eq $wide -or $support.ParentNode.LocalName -cne 'UserControl') {
    throw 'Support-card visual states need their own direct native control host.'
}
foreach ($target in @('SecondarySupportColumn.Width', 'AboutCard.(Grid.Column)', 'AboutCard.(Grid.Row)')) {
    if ($null -eq $wide.SelectSingleNode(".//ui:Setter[@Target='$target']", $ns)) {
        throw "Missing wide support-card layout setter: $target"
    }
}
$email = $shell.SelectSingleNode('//ui:HyperlinkButton[@x:Uid="Shell_PrivacyEmailLink"]', $ns)
if ($email.GetAttribute('Click') -cne 'PrivacyEmail_Click' -or $email.HasAttribute('NavigateUri')) {
    throw 'Email must use one checked native launcher, without an unchecked second NavigateUri launch.'
}
if ($null -eq $shell.SelectSingleNode('//ui:Button[@Click="CopyContactEmail_Click"]', $ns)) {
    throw 'Provide an explicit copy-address action even without a mail handler.'
}
$output = $shell.SelectSingleNode('//*[@x:Name="OutputDispositionControl"]', $ns)
if ($null -eq $output.SelectSingleNode('ui:RadioButton[@x:Uid="Shell_ReplaceOriginals"]', $ns)) {
    throw 'Output must offer replacing originals without a retained backup.'
}
$workspace = $shell.SelectSingleNode('//*[@x:Name="WorkflowContent"]/ancestor::ui:Grid[@x:Name="LandingPage"]', $ns)
if ($null -eq $workspace) { throw 'Review and processing must stay beside the persistent branded landing sidebar.' }
$brand = $workspace.SelectSingleNode('ui:Grid[@x:Name="BrandSidebar"]', $ns)
if ($null -eq $brand -or $workspace.ParentNode.HasAttribute('Visibility')) {
    throw 'The shared brand frame must remain mounted throughout folder work.'
}
Write-Output 'PASS: selectable help, native header input, responsive support cards, contact fallback and output choices.'
