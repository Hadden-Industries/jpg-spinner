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
foreach ($language in @('en-US', 'en-GB', 'ru')) {
    $resources = [Xml.XmlDocument]::new(); $resources.Load((Join-Path $root "src/JpgSpinner.App/Strings/$language/Resources.resw"))
    foreach ($key in @('Shell_ParallelPhotos.Header', 'Shell_ParallelPhotosDescription.Text', 'Shell_ParallelPhotosSummary')) {
        if ([string]::IsNullOrWhiteSpace($resources.SelectSingleNode("/root/data[@name='$key']/value").InnerText)) { throw "Missing $language parallel setting: $key" }
    }
}
Write-Output 'PASS: editable, localized whole-photo parallelism control; runtime behavior has separate native proof.'
