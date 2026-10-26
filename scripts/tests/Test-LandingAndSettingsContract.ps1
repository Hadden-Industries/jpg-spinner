<# Checks the user-facing separation before the native XAML build: preferences
   belong to Settings, a clean landing page offers folder selection, and the
   original interactive logo remains keyboard accessible. Rendering and input
   acceptance are verified on the development app, not inferred from XML. #>
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
$settings = $shell.SelectSingleNode('//ui:Grid[@x:Name="SettingsView"]', $ns)
if ($null -eq $settings) { throw 'A dedicated Settings page is required.' }
foreach ($identifier in @('TraversalScope', 'OutputDisposition', 'EdgeHandling', 'ScanOrganization')) {
    $controls = $shell.SelectNodes("//*[@AutomationProperties.AutomationId='$identifier']", $ns)
    if ($controls.Count -ne 1) { throw "Expected one accessible $identifier preference." }
    $parent = $controls.Item(0).ParentNode
    while ($null -ne $parent -and $parent -ne $settings) { $parent = $parent.ParentNode }
    if ($parent -ne $settings) { throw "$identifier must be on the Settings page." }
}
$landing = $shell.SelectSingleNode('//*[@x:Name="LandingPage"]', $ns)
if ($null -eq $landing) { throw 'A separate branded landing page is required.' }
$folderPicker = $landing.SelectSingleNode('.//ui:Button[@AutomationProperties.AutomationId="SourceFolderPicker"]', $ns)
if ($null -eq $folderPicker) { throw 'Landing must offer the native folder picker.' }
if ($landing.SelectNodes('.//ui:ComboBox|.//ui:RadioButtons', $ns).Count -ne 0) { throw 'Landing must not display processing preferences.' }
$logo = $landing.SelectSingleNode('.//ui:Button[@AutomationProperties.AutomationId="SpinLogo"]', $ns)
if ($null -eq $logo -or $logo.GetAttribute('Click') -cne 'Logo_Click') { throw 'The logo must offer a native keyboard-accessible spin action.' }
# These are the native Button template's pointer-state keys, independently
# defined by WinUI. Keep the Easter egg visually undisclosed on hover/press
# without replacing the native keyboard focus or invocation behaviour.
foreach ($resourceKey in @('ButtonBackgroundPointerOver', 'ButtonBackgroundPressed')) {
    $brush = $logo.SelectSingleNode("ui:Button.Resources/ui:SolidColorBrush[@x:Key='$resourceKey']", $ns)
    if ($null -eq $brush -or $brush.GetAttribute('Color') -cne 'Transparent') {
        throw "The logo must have a transparent $resourceKey background."
    }
}
if ($logo.GetAttribute('UseSystemFocusVisuals') -ceq 'False' -or $null -ne $logo.SelectSingleNode('ui:Button.Template', $ns)) {
    throw 'The logo must preserve the native keyboard focus and button template.'
}
if ($shell.SelectNodes('//*[@AutomationProperties.AutomationId="OpenSettings"]', $ns).Count -ne 1) { throw 'One discoverable app Settings entry is required.' }
foreach ($article in @('CropExplanationView', 'ProgressiveExplanationView', 'PrivacyView')) {
    if ($shell.SelectNodes("//ui:Grid[@x:Name='$article']", $ns).Count -ne 1) { throw "Restore the dedicated $article article." }
}
foreach ($topic in @('Crop', 'Progressive', 'Privacy')) {
    if ($settings.SelectNodes(".//ui:HyperlinkButton[@Tag='$topic']", $ns).Count -ne 1) { throw "Settings must link to $topic." }
}
$carousel = $shell.SelectSingleNode('//ui:FlipView[@x:Name="ProgressiveExampleCarousel"]', $ns)
if ($null -eq $carousel -or $carousel.SelectNodes('ui:FlipViewItem', $ns).Count -ne 3) {
    throw 'The progressive explanation must contain the three-image level carousel.'
}
foreach ($link in $shell.SelectNodes('//ui:HyperlinkButton[@NavigateUri]', $ns)) {
    $uri = [Uri]$link.GetAttribute('NavigateUri')
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -cne 'https') { throw 'Privacy web links require absolute HTTPS URLs.' }
}
foreach ($language in @('en-US', 'en-GB', 'ru')) {
    $resources = [Xml.XmlDocument]::new()
    $resources.Load((Join-Path $repositoryRoot "src/JpgSpinner.App/Strings/$language/Resources.resw"))
    foreach ($attribute in $resources.SelectNodes('//@*')) {
        # XML Namespaces reserves this namespace exclusively for the xml
        # prefix. MakePRI rejects aliases that XmlDocument can serialize.
        if ($attribute.NamespaceURI -ceq 'http://www.w3.org/XML/1998/namespace' -and $attribute.Prefix -cne 'xml') {
            throw "Invalid reserved XML namespace prefix in $language resources."
        }
    }
}
Write-Output 'PASS: clean landing, dedicated accessible Settings and interactive logo source contract; no rendered-layout claim.'
