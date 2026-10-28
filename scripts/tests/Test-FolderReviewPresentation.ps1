<# Native XAML contract for the streamlined review workspace. Runtime picker
   and read-only analysis behavior are qualified separately on the Dev app. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$shell = [Xml.XmlDocument]::new()
$shell.Load((Join-Path $root 'src/JpgSpinner.App/MainWindow.xaml'))
$ns = [Xml.XmlNamespaceManager]::new($shell.NameTable)
$ns.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
$ns.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
if ($shell.SelectNodes('//ui:Button[@AutomationProperties.AutomationId="AnalyzeSelectedSource" or @x:Uid="Shell_Analyze"]', $ns).Count -ne 0) {
    throw 'Folder selection must not require a separate Analyze confirmation.'
}
$list = $shell.SelectSingleNode('//ui:ListView[@x:Name="BatchImageList"]', $ns)
if ($null -eq $list -or $list.GetAttribute('ItemsSource') -cne '{x:Bind ViewModel.Rows, Mode=OneWay}') {
    throw 'Use one bounded native list for immutable review rows.'
}
if ($null -ne $list.SelectSingleNode('ancestor::ui:ScrollViewer', $ns)) {
    throw 'The photo list must own scrolling so its rows remain virtualized.'
}
$trimming = $list.SelectSingleNode('.//ui:InfoBar[@IsOpen="{x:Bind HasEdgeTrimming, Mode=OneWay}"]', $ns)
if ($null -eq $trimming -or $null -eq $trimming.SelectSingleNode('.//ui:Button[@AutomationProperties.AutomationId="AcknowledgeEdgeTrimming"]', $ns)) {
    throw 'Only photos with actual edge trimming should expose trimming details and consent.'
}
$apply = $shell.SelectSingleNode('//ui:Button[@AutomationProperties.AutomationId="StartProcessing"]', $ns)
if ($null -eq $apply -or $apply.GetAttribute('Style') -cne '{StaticResource AccentButtonStyle}' -or $apply.GetAttribute('Visibility') -cne '{x:Bind ReviewRegionVisibility, Mode=OneWay}') {
    throw 'Applying reviewed corrections is the single prominent review action.'
}
foreach ($language in @('en-US', 'en-GB', 'ru')) {
    $resources = [Xml.XmlDocument]::new()
    $resources.Load((Join-Path $root "src/JpgSpinner.App/Strings/$language/Resources.resw"))
    if ($resources.SelectNodes('/root/data[starts-with(@name,"Shell_Analyze.") or @name="Shell_InitialInstruction.Text"]').Count) {
        throw "Remove obsolete Analyze prompts from $language."
    }
}
Write-Output 'PASS: streamlined review, one native list, contextual edge consent and no redundant Analyze confirmation.'
