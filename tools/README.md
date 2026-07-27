# Image conversion tools

Two converters for baking images into firmware flash as LVGL-consumable arrays.

## Icons — `png_to_lvgl.js`

```bash
node png_to_lvgl.js <input.png> <symbol> [W_MACRO] [H_MACRO] [--tint=RRGGBB | --no-tint]
```

Converts an alpha PNG to RGB565A8 (planar: `w*h` RGB565 pixels followed by
`w*h` alpha bytes). Default tint is white (`0xFFFFFF`) — necessary for
black-on-transparent source PNGs (e.g. Lucide icons) to render visibly.
Splice the output into `firmware/src/icons.h` and wrap it with
`init_icon_dsc_rgb565a8()` in `ui.cpp`. Use this for anything that overlays a
non-uniform background (the battery indicator sits over all three tiles).

## Backgrounds — `img_to_lvgl.py`

```bash
python img_to_lvgl.py <input.jpg> <symbol> <MACRO_PREFIX> --size WxH --out <output.h>
```

Converts any Pillow-readable photo to a flat **opaque** RGB565 array (no
alpha plane — this is for a full-screen background, not an overlaid icon).
Source images live in `firmware/assets/backgrounds/`; output goes to
`firmware/src/backgrounds.h` (generated, do not hand-edit). Wrap with
`init_bg_dsc_rgb565()` in `ui.cpp` (a plain-RGB565 variant of the icon
helper above, since there's no alpha plane to size for).
