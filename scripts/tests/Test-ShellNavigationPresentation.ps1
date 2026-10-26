<# Guard the reported presentation regressions at their authored XAML seam.
   Native Frame input tests remain separate: removing visible history controls
   must not remove keyboard/mouse navigation. This is not a rendering test. #>
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
$problems = [Collections.Generic.List[string]]::new()
$titleBar = $shell.SelectSingleNode('//ui:TitleBar', $ns)
if ($null -eq $titleBar -or $titleBar.GetAttribute('IsBackButtonVisible') -ceq 'True' -or
    $titleBar.SelectNodes('.//*[@Icon="Forward" or @Icon="Back"]', $ns).Count -ne 0) {
    $problems.Add('The title bar must not duplicate page-level Back navigation or display a Forward command.')
}
$root = $shell.SelectSingleNode('//ui:Grid[@x:Name="ShellRoot"]', $ns)
if ($null -eq $root -or $root.GetAttribute('KeyboardAcceleratorPlacementMode') -cne 'Hidden') {
    $problems.Add('Root navigation accelerators must not produce a tooltip over landing-page content.')
}
$publisherLink = $shell.SelectSingleNode('//*[@x:Name="LandingPage"]//ui:HyperlinkButton[@NavigateUri="https://haddenindustries.com/"]', $ns)
if ($null -eq $publisherLink -or $publisherLink.GetAttribute('Content') -cne '© Hadden Industries Ltd') {
    $problems.Add('The landing-page publisher credit must be a native link to the publisher website.')
}
if ($problems.Count -ne 0) { throw ($problems -join [Environment]::NewLine) }
Write-Output 'PASS: page-level navigation, hidden root shortcut tooltip, and native publisher link.'
