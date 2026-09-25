# Vibescape Venus splash artwork

Generated using the built-in image generator from the user's reference `C:/Users/winbo/Downloads/images.jpg` (vintage Adobe Illustrator 8 splash). The reference informed composition and serif typography; the result contains only the Vibescape name.

- `vibescape-venus-master.png`: native generated artwork, 2206 × 713.
- `vibescape-venus-700x220.png`: approved artwork for the 700 × 220 opening splash slot; Lanczos resized to fill and center cropped to the exact dimensions.
- The approved export is also stored as `../start-splash.png`, the active opening splash resource. The master and prompts are retained here for future artwork changes.

Export command (ImageMagick):

```powershell
magick vibescape-venus-master.png -filter Lanczos -resize '700x220^' -gravity center -extent 700x220 vibescape-venus-700x220.png
```

## Initial generation prompt

Create a NEW illustrated application splash banner inspired by the attached vintage Adobe Illustrator 8 splash image. The attachment is a visual reference for the cropped Botticelli Venus face, flowing hair, painted illustration texture, and graceful serif typography only. Do not reproduce Adobe branding, logos, version numbers, credits, loading text, or legal text.

FORMAT: very wide, shallow horizontal banner, 35:11 aspect ratio, intended for 700 by 220 pixels, ideally render at 2240 by 704 pixels. Edge-to-edge opaque illustration, no border, no mockup. Compose deliberately for a wide strip.

SUBJECT AND FRAMING: feature ONLY the head and flowing hair of Botticelli's adult Venus, reimagined in an illustrated post-apocalyptic style. Her face occupies the left-center of the banner, with forehead, eyes, nose, mouth and chin readable; no full body, no shell, no other people. Preserve the reference's beautiful melancholy green-eyed expression, subtle head tilt, Renaissance facial proportions and copper-gold wavy hair. Let long ribbonlike locks flow across the width of the banner and beneath the title area.

HALF BIONIC AND SCARRED: one entire half of her face is recognizably human, the other half elegantly reconstructed with exposed articulated aged-brass and dark-steel facial plates, delicate mechanical joints and inset green optical components, while retaining her graceful facial structure. This must visibly read as HALF BIONIC, not just a small gadget at her temple. Give the human half a clearly legible healed diagonal facial scar crossing the eyebrow and cheek. No fresh injuries or gore. Salvaged mechanical patina and etched wear convey the post-apocalyptic setting. She should remain soulful, dignified and beautiful rather than monstrous.

ILLUSTRATION STYLE: finely drawn Renaissance-inspired face with confident expressive ink linework and richly textured painted color, similar in feeling to the vintage splash reference. Flowing decorative hair, muted warm gold, copper, weathered cream, olive and soft blue-gray accents. Flat illustrative artistry, not photorealism or 3D rendering. Background can be softly distressed warm parchment and simplified sweeping hair shapes; keep it sufficiently quiet in the upper-right for excellent title readability.

TEXT: exactly one word, "Vibescape", spelled V-i-b-e-s-c-a-p-e, capital V and all remaining letters lowercase. Place it prominently in the UPPER-RIGHT CORNER, with comfortable top and right margins. Use an elegant tall high-contrast serif with graceful tapering strokes and sharp flared serifs, visually reminiscent of the "Illustrator" lettering in the attached reference, in a rich dark aubergine or near-black ink. Make the lettering crisp and large enough to remain legible at 700 by 220. Keep all text away from the eyes and face.

No other text whatsoever. No Adobe logo, no Illustrator name, no version number, no copyright line, no watermark.

## Panoramic composition prompt

Turn this exact approved-direction artwork into a true ultra-wide splash-screen strip. The input's face, bionic detail, healed scars, copper flowing hair, colors, texture and the exact word "Vibescape" in its existing elegant dark-purple serif are all correct: preserve them.

Change only the composition and canvas proportions. The final canvas MUST be about 3.18 TIMES AS WIDE AS IT IS TALL: 35:11, the proportions of 700 x 220, ideally 2240 x 704. This is much wider and shallower than the input; do not return a normal landscape rectangle. Outpaint horizontally with more flowing hair and warm parchment. Fit the whole face, including forehead and chin, into the banner's height without distortion, with the face on the left third. Keep hair sweeping across the lower and middle portions of the banner.

Reposition the exact existing "Vibescape" title into the UPPER RIGHT corner of the expanded panorama with generous safe margins, at least 6 percent of canvas height away from the top and 4 percent of canvas width from the right edge. Maintain its beautiful serif letterforms, casing and high legibility, and keep it clear of the face and bionic eye.

Keep the scars obvious, the face visibly half bionic, and the hair flowing. No additional text, logos, borders or new objects. Do not change facial identity or repaint into a different style. Final output should be an unusually long horizontal banner, not a poster.
