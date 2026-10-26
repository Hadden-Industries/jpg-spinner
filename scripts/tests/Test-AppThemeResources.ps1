<# WinUI selects one theme dictionary; it does not fill missing Dark keys
   from Default. Every branded brush must exist in each supported theme.
   https://github.com/microsoft/microsoft-ui-xaml/blob/main/docs/design-notes/resources.md #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$document = [Xml.XmlDocument]::new()
$document.Load((Join-Path $root 'src/JpgSpinner.App/App.xaml'))
$ns = [Xml.XmlNamespaceManager]::new($document.NameTable)
$ns.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
$ns.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
$themes = $document.SelectNodes('//ui:ResourceDictionary.ThemeDictionaries/ui:ResourceDictionary', $ns)
$allBrushKeys = @($themes.SelectNodes('ui:SolidColorBrush/@x:Key', $ns).Value | Sort-Object -Unique)
foreach ($name in @('Light', 'Dark', 'HighContrast')) {
    $theme = $document.SelectSingleNode("//ui:ResourceDictionary.ThemeDictionaries/ui:ResourceDictionary[@x:Key='$name']", $ns)
    if ($null -eq $theme) { throw "Declare the $name theme explicitly." }
    $keys = @($theme.SelectNodes('ui:SolidColorBrush/@x:Key', $ns).Value | Sort-Object -Unique)
    if (Compare-Object -ReferenceObject $allBrushKeys -DifferenceObject $keys) { throw "The $name theme omits a branded brush used by the app." }
}
Write-Output 'PASS: complete branded brush coverage for Light, Dark and HighContrast.'
