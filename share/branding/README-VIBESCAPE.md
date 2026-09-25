# Vibescape Windows icon

The orange "Vi" design was supplied by the user for this personal fork.
`vibescape-reference.jpg` is an unchanged copy of the supplied JPG.
Existing Inkscape branding and copyright files remain available alongside it.

- `vibescape.png`: transparent production artwork prepared from the supplied design.
- `vibescape.ico`: embedded Windows icon, with 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 pixel images.
- `windows/<size>x<size>/vibescape.png`: raster icons for the running Windows GTK application.

The EXE and console launchers embed the ICO through `src/inkscape.rc`.
Retained NSIS/WiX configuration references the same ICO; no binary distribution is planned.
The runnable folder also receives `share/inkscape/branding/vibescape.ico`,
which can be selected manually when making a shortcut.

These changes do not create or update Windows shortcuts, change file associations,
or modify an independently installed Inkscape.

## Regenerating the icon formats

From this directory with ImageMagick on PATH:

```powershell
magick vibescape.png -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 vibescape.ico
foreach ($size in 16,22,24,32,48,256) {
    magick vibescape.png -resize "${size}x${size}" "windows/${size}x${size}/vibescape.png"
}
```

The resulting files are checked-in build inputs; ImageMagick and an image
generation service are not required to compile Vibescape.

## Artwork preparation

The built-in image-generation tool prepared the transparent PNG. It is a
generated adaptation of the supplied design, not a pixel-exact extraction.
The later cleanup candidate was not selected.

Preparation prompt:

> Edit this supplied image into a production Windows application icon asset.
> Use case: background-extraction. Keep the exact supplied orange-to-red
> rounded-square badge, its golden top/left rim, bevel, subtle texture,
> shadowed cream-white capital V and lowercase i, text 'Vi', relative lettering
> positions, proportions, colors, and shading. Remove only the surrounding
> white rectangular background. Do not redesign or reinterpret the logo and
> do not add details or text. Deliver the badge centered on a square canvas,
> filling about 94 percent of the canvas, with genuinely transparent alpha
> outside the rounded silhouette, including clean transparent corners.
> Preserve the internal letter counters and white lettering as opaque.
> No ground plane or outside drop shadow, no extra white margin, no mockup.
> This is the user's approved design to use faithfully as an application icon.
