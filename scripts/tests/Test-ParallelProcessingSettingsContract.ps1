<# Public wiring guard; native batch tests establish real concurrency,
   memory admission, cancellation and transaction outcome semantics. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$xml = [Xml.XmlDocument]::new(); $xml.Load((Join-Path $root 'src/JpgSpinner.App/MainWindow.xaml'))
$ns = [Xml.XmlNamespaceManager]::new($xml.NameTable)
$ns.AddNamespace('ui', 'http://schemas.microsoft.com/winfx/2006/xaml/presentation')
$ns.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
$slider = $xml.SelectSingleNode('//ui:Grid[@x:Name="SettingsView"]//ui:Slider[@x:Name="MaximumConcurrentImagesControl"]', $ns)
if ($null -eq $slider) { throw 'Settings must expose a functional parallel-photo limit.' }
if ($slider.GetAttribute('Minimum') -cne '1' -or $slider.GetAttribute('TickFrequency') -cne '1' -or $slider.GetAttribute('StepFrequency') -cne '1' -or $slider.GetAttribute('SnapsTo') -cne 'StepValues') { throw 'The parallel limit must select positive whole photos.' }
if ($slider.GetAttribute('ValueChanged') -cne 'MaximumConcurrentImages_ValueChanged' -or $slider.GetAttribute('IsEnabled') -cne '{x:Bind CanEditProcessingSettings, Mode=OneWay}') { throw 'Use the shared editable-processing gate and native value-change route.' }
$automatic = $xml.SelectSingleNode('//ui:Grid[@x:Name="SettingsView"]//ui:ToggleSwitch[@x:Name="AutomaticMemoryBudgetControl"]', $ns)
$custom = $xml.SelectSingleNode('//ui:Grid[@x:Name="SettingsView"]//ui:NumberBox[@x:Name="ProcessingMemoryBudgetControl"]', $ns)
if ($null -eq $automatic -or $null -eq $custom) { throw 'Settings must expose Automatic and an optional custom processing-memory budget.' }
if ($automatic.GetAttribute('IsOn') -cne 'True' -or $automatic.GetAttribute('IsEnabled') -cne '{x:Bind CanEditProcessingSettings, Mode=OneWay}' -or $automatic.GetAttribute('Toggled') -cne 'AutomaticMemoryBudget_Toggled') { throw 'Automatic is the default and must respect the shared processing gate.' }
if ($custom.GetAttribute('Minimum') -cne '1' -or $custom.GetAttribute('Visibility') -cne '{x:Bind CustomMemoryBudgetVisibility, Mode=OneWay}' -or $custom.GetAttribute('ValueChanged') -cne 'ProcessingMemoryBudget_ValueChanged') { throw 'Custom memory uses a positive native numeric input, visible only when selected.' }
foreach ($language in @('en-US', 'en-GB', 'ru')) {
    $resources = [Xml.XmlDocument]::new(); $resources.Load((Join-Path $root "src/JpgSpinner.App/Strings/$language/Resources.resw"))
    foreach ($key in @('Shell_ParallelPhotos.Header', 'Shell_ParallelPhotosDescription.Text', 'Shell_ParallelPhotosSummary', 'Shell_MemoryBudget.Header', 'Shell_AutomaticMemoryBudget.Header', 'Shell_ProcessingMemoryBudget.Header', 'Shell_MemoryBudgetDescription.Text', 'Shell_MemoryBudgetAutomaticSummary', 'Shell_MemoryBudgetCustomSummary')) {
        if ([string]::IsNullOrWhiteSpace($resources.SelectSingleNode("/root/data[@name='$key']/value").InnerText)) { throw "Missing $language parallel setting: $key" }
    }
}
Write-Output 'PASS: editable, localized whole-photo parallelism control; runtime behavior has separate native proof.'
