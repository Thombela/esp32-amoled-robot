# Image conversion tools

Two converters for baking images into firmware flash as LVGL-consumable arrays.
Source images live in `assets/` (icons/logos in `assets/apps/`, wallpapers in
`assets/background/`); generated output goes to `firmware/src/*.h` (do not
hand-edit those, they're regenerated from `assets/`).

## Icons — `png_to_lvgl.js`

```bash
cd tools && npm install --no-save pngjs   # first time only, gitignored
node png_to_lvgl.js <input.png> <symbol> [W_MACRO] [H_MACRO] [--tint=RRGGBB | --no-tint]
```

Converts an alpha PNG to RGB565A8 (planar: `w*h` RGB565 pixels followed by
`w*h` alpha bytes). Default tint is white (`0xFFFFFF`) — necessary for
black-on-transparent source PNGs (e.g. Lucide icons) to render visibly; pass
`--no-tint` for full-color art that should render as-is (app/brand logos —
their PNGs already carry their own colors and rounded-square shape, alpha and
all, so no clip-mask wrapper is needed at the LVGL side either). Wrap the
result with `init_icon_dsc_rgb565a8()` in `ui.cpp`. Use this for anything
that overlays a non-uniform background (the battery indicator, app icons).

Unlike `img_to_lvgl.py` below, this script's output has **no** `#pragma
once`/`#include` preamble and no `--append` flag — it was written to be
spliced into an existing header (`icons.h`). When building a new standalone
header out of several of its outputs (e.g. `app_icons.h`), redirect the first
call to create the file, `>>`-append the rest, then manually prepend
`#pragma once` / `#include <stdint.h>` once.

## Backgrounds — `img_to_lvgl.py`

```bash
python img_to_lvgl.py <input.jpg> <symbol> <MACRO_PREFIX> --size WxH --out <output.h>
```

Converts any Pillow-readable photo to a flat **opaque** RGB565 array (no
alpha plane — this is for a full-screen background, not an overlaid icon).
Output goes to `firmware/src/backgrounds.h`. Wrap with
`init_bg_dsc_rgb565()` in `ui.cpp` (a plain-RGB565 variant of the icon
helper above, since there's no alpha plane to size for).

Pass `--append` to add a second (or third...) image's array to an existing
`--out` file instead of overwriting it (this one *does* manage its own
`#pragma once`/`#include` preamble, adding it only on the first, non-append
call) — used to bake the Home and Library backgrounds into one file.
