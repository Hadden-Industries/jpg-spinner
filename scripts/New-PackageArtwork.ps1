<#
.SYNOPSIS
Produces Windows PNG resource variants from the retained owned raster masters.
.DESCRIPTION
Windows-only System.Drawing/GDI+ owns decoding, high-quality resampling and PNG
encoding. No custom codec, AI branding, background removal or remote download.
Generated sizes follow the accepted manifest contract. Output files are replaced
only by explicit invocation; the preserved masters are never modified. Byte
determinism is qualified on the recorded Windows/.NET/GDI+ lane, not universally.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputDirectory = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Package artwork generation requires the qualified Windows GDI+ lane.' }
Add-Type -AssemblyName System.Drawing
$root = [IO.Path]::GetFullPath($RepositoryRoot)
if ([string]::IsNullOrEmpty($OutputDirectory)) { $OutputDirectory = Join-Path $root 'src/JpgSpinner.App/Assets' }
$destination = [IO.Path]::GetFullPath($OutputDirectory)
[void][IO.Directory]::CreateDirectory($destination)
$sourceDirectory = Join-Path $root 'assets-source'

function Write-ResampledPackageArtwork {
    param([string]$SourceName, [string]$DestinationName, [int]$Width, [int]$Height)
    $source = [Drawing.Image]::FromFile((Join-Path $sourceDirectory $SourceName))
    try {
        # Fill with the existing orange plate before compositing its raster.
        # Historical edge alpha is blended onto its own plate, not black halos.
        $bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([Drawing.Color]::FromArgb(255,240,91,41))
                $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceOver
                $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $attributes = [Drawing.Imaging.ImageAttributes]::new()
                try {
                    # Native edge wrapping avoids transparent sampling outside
                    # the original raster without inventing a resampling filter.
                    $attributes.SetWrapMode([Drawing.Drawing2D.WrapMode]::TileFlipXY)
                    $rectangle = [Drawing.Rectangle]::new(0,0,$Width,$Height)
                    $graphics.DrawImage($source, $rectangle, 0,0,$source.Width,$source.Height,
                        [Drawing.GraphicsUnit]::Pixel, $attributes)
                } finally { $attributes.Dispose() }
            } finally { $graphics.Dispose() }
            $bitmap.Save((Join-Path $destination $DestinationName), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
    } finally { $source.Dispose() }
}

foreach ($scale in @(100,200,400)) {
    Write-ResampledPackageArtwork 'JpgSpinnerAppIcon.png' "Square44x44Logo.scale-$scale.png" (44*$scale/100) (44*$scale/100)
    Write-ResampledPackageArtwork 'JpgSpinnerMediumTile.png' "Square150x150Logo.scale-$scale.png" (150*$scale/100) (150*$scale/100)
    Write-ResampledPackageArtwork 'JpgSpinnerWideTile.png' "Wide310x150Logo.scale-$scale.png" (310*$scale/100) (150*$scale/100)
    Write-ResampledPackageArtwork 'JpgSpinnerSplashScreen.png' "SplashScreen.scale-$scale.png" (620*$scale/100) (300*$scale/100)
}
Write-ResampledPackageArtwork 'JpgSpinnerAppIcon.png' 'StoreLogo.png' 50 50
foreach ($size in @(16,20,24,30,32,36,40,48,60,64,72,80,96,256)) {
    foreach ($theme in @('', '_altform-unplated', '_altform-lightunplated')) {
        # Owned branding uses an orange plate in all themes. Theme qualifiers
        # therefore legitimately have identical bytes; no forced white/black art.
        Write-ResampledPackageArtwork 'JpgSpinnerAppIcon.png' "Square44x44Logo.targetsize-$size$theme.png" $size $size
    }
}
Write-Output "Generated 55 owned Windows PNG resource variants at $destination."
