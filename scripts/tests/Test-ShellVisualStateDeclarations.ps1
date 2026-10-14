<#
.SYNOPSIS
Checks the shell's authored visual-state host and approved width declarations.
.DESCRIPTION
This source contract uses .NET XML parsing, without compiling or activating the
app. It does not validate XAML types, resolve bindings, or prove that the layout
renders correctly. Those remain native XAML-compiler and interactive UIA duties.
The in-memory negative controls cannot modify the working tree or user settings.
#>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-ShellVisualStateDeclarations {
    param([Parameter(Mandatory)][System.Xml.XmlDocument]$Document)
    $namespaces = [System.Xml.XmlNamespaceManager]::new($Document.NameTable)
    $namespaces.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
    $namespaces.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
    $workflowRoots = $Document.SelectNodes('//ui:Grid[@x:Name="WorkflowContent"]', $namespaces)
    if ($workflowRoots.Count -ne 1) { throw 'Exactly one WorkflowContent grid is required.' }
    $workflowRoot = $workflowRoots.Item(0)
    # This is the selected shell composition, not a general XAML validator.
    # Native state triggers belong on the control's content root; nesting them
    # in a ContentControl's ordinary content caused the retained live failure.
    # https://learn.microsoft.com/windows/apps/develop/ui/layouts-with-xaml
    if ($workflowRoot.ParentNode.LocalName -cne 'UserControl' -or
        $workflowRoot.ParentNode.NamespaceURI -cne $namespaces.LookupNamespace('ui')) {
        throw 'WorkflowContent must be the UserControl content root.'
    }
    $stateGroups = $workflowRoot.SelectNodes('ui:VisualStateManager.VisualStateGroups', $namespaces)
    if ($stateGroups.Count -ne 1) { throw 'VisualStateGroups must be declared directly on WorkflowContent.' }
    $minimumWidthByState = [ordered]@{ Narrow = 0; Compact = 640; Wide = 1008 }
    foreach ($state in $minimumWidthByState.GetEnumerator()) {
        $states = $stateGroups.Item(0).SelectNodes(
            "ui:VisualStateGroup/ui:VisualState[@x:Name='$($state.Key)']", $namespaces)
        if ($states.Count -ne 1) { throw "Exactly one $($state.Key) visual state is required." }
        $triggers = $states.Item(0).SelectNodes('ui:VisualState.StateTriggers/ui:AdaptiveTrigger', $namespaces)
        if ($triggers.Count -ne 1) { throw "$($state.Key) requires one native adaptive trigger." }
        # XML numeric conversion permits equivalent forms such as 640.0. Do not
        # invent a numeric grammar or mistake literal spelling for the value.
        $minimumWidth = [System.Xml.XmlConvert]::ToDouble($triggers.Item(0).GetAttribute('MinWindowWidth'))
        if ($minimumWidth -ne $state.Value) {
            throw "$($state.Key) minimum width must be $($state.Value)."
        }
    }
}

function Assert-RejectedShellDeclaration {
    param(
        [Parameter(Mandatory)][System.Xml.XmlDocument]$Document,
        [Parameter(Mandatory)][string]$ExpectedDiagnostic
    )
    $rejection = $null
    try { Assert-ShellVisualStateDeclarations -Document $Document }
    catch { $rejection = $_.Exception.Message }
    if ($null -eq $rejection) { throw "Defective shell declaration was accepted: $ExpectedDiagnostic" }
    if (-not $rejection.Contains($ExpectedDiagnostic, [StringComparison]::Ordinal)) {
        throw "Unexpected shell declaration refusal: $rejection"
    }
}

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$shell = [System.Xml.XmlDocument]::new()
$shell.Load((Join-Path $repositoryRoot 'src/JpgSpinner.App/MainWindow.xaml'))
Assert-ShellVisualStateDeclarations -Document $shell

# Recreate only the previous host-ownership defect in an independent DOM.
# XML construction owns syntax; the actual shell file remains unchanged.
$wrongHost = [System.Xml.XmlDocument]$shell.CloneNode($true)
$hostControl = $wrongHost.GetElementsByTagName('UserControl').Item(0)
if ($null -eq $hostControl) { throw 'The positive fixture has no UserControl host.' }
$replacement = $wrongHost.CreateElement('ContentControl', $hostControl.NamespaceURI)
foreach ($attribute in $hostControl.Attributes) {
    [void]$replacement.Attributes.Append($attribute.CloneNode($true))
}
while ($hostControl.HasChildNodes) { [void]$replacement.AppendChild($hostControl.FirstChild) }
[void]$hostControl.ParentNode.ReplaceChild($replacement, $hostControl)
Assert-RejectedShellDeclaration -Document $wrongHost -ExpectedDiagnostic 'UserControl content root'

# Approved breakpoints are an independent design oracle, not values derived
# from production XAML. Detect a plausible one-pixel boundary regression.
$wrongWidth = [System.Xml.XmlDocument]$shell.CloneNode($true)
$fixtureNamespaces = [System.Xml.XmlNamespaceManager]::new($wrongWidth.NameTable)
$fixtureNamespaces.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
$fixtureNamespaces.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
$compactTrigger = $wrongWidth.SelectSingleNode(
    "//ui:VisualState[@x:Name='Compact']/ui:VisualState.StateTriggers/ui:AdaptiveTrigger", $fixtureNamespaces)
if ($null -eq $compactTrigger) { throw 'The positive fixture requires a compact-width trigger.' }
$compactTrigger.SetAttribute('MinWindowWidth', '641')
Assert-RejectedShellDeclaration -Document $wrongWidth -ExpectedDiagnostic 'Compact minimum width must be 640'

# A semantic-value guard must accept equivalent XML numeric representations.
$compactTrigger.SetAttribute('MinWindowWidth', '640.0')
Assert-ShellVisualStateDeclarations -Document $wrongWidth

Write-Output 'PASS: shell visual-state source declarations and wrong-host/width negative controls; no rendered-layout acceptance claimed.'
