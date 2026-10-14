<# Decode required PNGs independently of the generator. This checks authored
   resource dimensions and brand preservation, not shell rendering acceptance. #>
[CmdletBinding()]
param([string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assets = Join-Path $RepositoryRoot 'src/JpgSpinner.App/Assets'
# Independent dimensions from Microsoft's Windows app-icon construction guide.
$required = [ordered]@{
    'Square44x44Logo.scale-100.png' = @(44,44)
    'Square44x44Logo.scale-200.png' = @(88,88)
    'Square44x44Logo.scale-400.png' = @(176,176)
    'Square150x150Logo.scale-100.png' = @(150,150)
    'Square150x150Logo.scale-200.png' = @(300,300)
    'Square150x150Logo.scale-400.png' = @(600,600)
    'StoreLogo.png' = @(50,50)
    'Wide310x150Logo.scale-100.png' = @(310,150)
    'Wide310x150Logo.scale-200.png' = @(620,300)
    'Wide310x150Logo.scale-400.png' = @(1240,600)
    'SplashScreen.scale-100.png' = @(620,300)
    'SplashScreen.scale-200.png' = @(1240,600)
    'SplashScreen.scale-400.png' = @(2480,1200)
}
foreach ($size in @(16,20,24,30,32,36,40,48,60,64,72,80,96,256)) {
    foreach ($theme in @('', '_altform-unplated', '_altform-lightunplated')) {
        $required["Square44x44Logo.targetsize-$size$theme.png"] = @($size,$size)
    }
}
foreach ($name in $required.Keys) {
    $path = Join-Path $assets $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required owned package artwork is absent: $name" }
    $bitmap = [Drawing.Bitmap]::new($path)
    try {
        $expected = $required[$name]
        if ($bitmap.Width -ne $expected[0] -or $bitmap.Height -ne $expected[1]) {
            throw "Package artwork has incorrect decoded dimensions: $name"
        }
        if ($bitmap.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) {
            throw "Package artwork is not native PNG: $name"
        }
        # Preserve the existing orange plate (opaque interior RGB 240/91/41).
        # Do not silently replace the owned branding or keep gray scaffold art.
        $corner = $bitmap.GetPixel(0,0)
        # Native bicubic/compositing quantizes the historical translucent edges
        # to 8-bit channels. A two-level palette tolerance covers that rounding,
        # not a different brand; dimensions and retained master bytes are exact.
        if ($corner.A -ne 255 -or [Math]::Abs([int]$corner.R - 240) -gt 2 -or
            [Math]::Abs([int]$corner.G - 91) -gt 2 -or [Math]::Abs([int]$corner.B - 41) -gt 2) {
            throw "Package artwork does not preserve the owned orange plate: $name"
        }
        if ($name.StartsWith('Square', [StringComparison]::Ordinal)) {
            # The independently inspected masters have a white glyph at their
            # center. A blank orange PNG must not satisfy the branding oracle.
            $center = $bitmap.GetPixel([int]($bitmap.Width / 2), [int]($bitmap.Height / 2))
            if ($center.R -lt 245 -or $center.G -lt 245 -or $center.B -lt 245) {
                throw "Package artwork lost the owned white glyph: $name"
            }
        }
    } finally { $bitmap.Dispose() }
}
Write-Output "PASS: $($required.Count) native PNG artwork dimensions and owned plate controls."
