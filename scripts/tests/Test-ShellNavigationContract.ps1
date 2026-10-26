<# Verifies the input routes converge on WinUI's history, rather than a second
   journal of visibility flags. Native navigation behaviour is owned by Frame;
   this source check does not claim mouse/keyboard acceptance on a live device. #>
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
if ($shell.SelectNodes('//ui:Frame[@x:Name="ShellFrame"]', $ns).Count -ne 1) {
    throw 'The shell requires one native Frame navigation journal.'
}
foreach ($key in @('Left', 'Right')) {
    if ($shell.SelectNodes("//ui:KeyboardAccelerator[@Key='$key']", $ns).Count -ne 1) {
        throw "Missing standard $key navigation accelerator."
    }
}
foreach ($key in @('Left', 'Right')) {
    if ($shell.SelectSingleNode("//ui:KeyboardAccelerator[@Key='$key']", $ns).GetAttribute('Modifiers') -cne 'Menu') {
        throw 'Arrow navigation must require Alt, leaving ordinary editing keys alone.'
    }
}
$implementation = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src/JpgSpinner.App/MainWindow.xaml.cpp') -Raw
# WinUI's XAML converter rejects GoBack/GoForward although they are defined
# VirtualKey values. Typed native setters support them without string parsing.
foreach ($key in @('GoBack', 'GoForward')) {
    if ($shell.SelectNodes("//ui:KeyboardAccelerator[@Key='$key']", $ns).Count -ne 0 -or
        -not $implementation.Contains("VirtualKey::$key")) {
        throw "Use the native typed setter for $key; its XAML spelling fails at startup."
    }
}
foreach ($requiredNativeRoute in @('IsXButton1Pressed', 'IsXButton2Pressed', 'CanGoBack', 'CanGoForward', 'GoBack()', 'GoForward()')) {
    if (-not $implementation.Contains($requiredNativeRoute)) { throw "Missing native route: $requiredNativeRoute" }
}
if ($implementation.Contains('settingsOpen_') -or $implementation.Contains('settingsArticleOpen_')) {
    throw 'Page selection must not keep a parallel visibility-based navigation state.'
}
Write-Output 'PASS: native journal and standard input wiring; live input acceptance is separate.'
