# Retained JPG Spinner artwork masters

These are byte-identical copies of the repository's existing owned artwork,
retained outside the shipping asset directory so legacy retirement cannot delete
the generation inputs. Repository licensing applies; no new brand, AI-generated
artwork or upstream scaffold assets are used in the generated product icons.

| Current master | Historical origin | SHA-256 |
| --- | --- | --- |
| `JpgSpinnerAppIcon.png` | `JPG Spinner/Assets/SmallLogo.targetsize-256.png` | `46c183aa5a12325c2a4732d5607ceea7f82753db9eee2cb0476c35bed9d0bea1` |
| `JpgSpinnerMediumTile.png` | `JPG Spinner/Assets/Square310x310Logo.scale-180.png` | `50386858f32e199b7b79471c8e82fdae63d640080a1af4837ad5965b2ff43246` |
| `JpgSpinnerWideTile.png` | `JPG Spinner/Assets/Wide310x150Logo.scale-180.png` | `e000714e07e5160a8b8261cb3cb12dd72f58f719d6766ce99e6e057c5c317e11` |
| `JpgSpinnerSplashScreen.png` | `JPG Spinner/Assets/SplashScreen.scale-180.png` | `4b1b4c3dea36af09614b489fd7f7cffc5b0e469f2d2a82ecf5be03df1c9fa8ce` |

The icon master is the existing 256px unpadded app-list rendition. The 558px
square tile, 558×270 wide tile and 1116×540 splash rendition are the highest
existing rasters for those contexts. Required larger scale variants necessarily
upsample these inputs; this does not manufacture new source detail. Transparent
historical edge pixels are composited onto their existing orange plate to avoid
dark halos. The plate is retained in light and dark target-size variants, rather
than silently redesigning the brand into an unplated glyph.

Regenerate with `pwsh -NoProfile -File scripts/New-PackageArtwork.ps1`, then
run `scripts/tests/Test-PackageArtwork.ps1` and inspect actual shell rendering.
Windows GDI+/System.Drawing owns PNG decoding, bicubic resampling and encoding.
Same-lane byte reproducibility must be measured; output equality across different
Windows/.NET/GDI+ versions is not promised. Do not edit generated PNGs by hand.
