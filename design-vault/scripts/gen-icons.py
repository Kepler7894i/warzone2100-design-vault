"""Builds src/designvault_icondata.cpp: the three small images the Design Vault adds to the design screen.

  marker    the floppy disk of the game's own "store design" icon without its button frame, shown in the corner of stored designs
  upgrade   a button in the style of the game's design-screen buttons (same frame, same palette) with an arrow pointing up
  upgradeh  the same, highlighted (mouse over / pressed)

They are cut from / painted in the colours of images/intfac/image_des_save(h).png from the game's own data, so the theme follows
the game. The result is one 128x128 atlas, stored as PNG bytes in the source, so that the exe needs no extra data files.

    python design-vault/scripts/gen-icons.py [path to base.wz]
Needs Pillow. Run it only when the artwork changes; the generated .cpp is part of patches/design-vault.patch.
"""
import io
import os
import sys
import zipfile
from collections import Counter
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))   # the repository root
BASE_WZ = sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100\data\base.wz"
OUT_CPP = os.path.join(ROOT, "src", "designvault_icondata.cpp")
PREVIEW = os.path.join(ROOT, "run", "art", "icons-preview.png")

ATLAS = 128
UPGRADE_POS = (0, 0)
UPGRADEH_POS = (0, 32)
MARKER_POS = (0, 64)
BUTTON_W, BUTTON_H = 45, 24
MARKER_SRC_BOX = (10, 2, 39, 22)   # the disk and its pointer inside the 45x24 icon (x0, y0, x1, y1 exclusive)
MARKER_SIZE = (22, 15)             # drawn 1:1, so it needs no scaling in the game


def load(z, name):
    return Image.open(io.BytesIO(z.read("images/intfac/" + name + ".png"))).convert("RGBA")


def colour_map(normal, hilite):
    """For each colour of the highlighted icon, the colour its pixel has in the normal icon."""
    pairs = {}
    for p, q in zip(hilite.getdata(), normal.getdata()):
        pairs.setdefault(p, Counter())[q] += 1
    return {k: v.most_common(1)[0][0] for k, v in pairs.items()}


def blank_button(icon):
    """The icon with its symbol painted over: frame and background only."""
    bg = Counter(icon.getdata()).most_common(1)[0][0]
    out = icon.copy()
    px = out.load()
    for y in range(2, 22):
        for x in range(9, 40):
            px[x, y] = bg
    return out


# The arrow, as rows of its width (centred): a head that widens by one pixel on each side per row, then a shaft.
ARROW_HEAD = [1, 3, 5, 7, 9, 11, 13, 15]
ARROW_SHAFT = [7] * 9


def arrow_mask():
    rows = ARROW_HEAD + ARROW_SHAFT
    width = max(rows)
    mask = set()
    for y, w in enumerate(rows):
        x0 = (width - w) // 2
        for x in range(x0, x0 + w):
            mask.add((x, y))
    return mask, width, len(rows)


def paint_arrow(button, palette):
    """palette: dict with light, base, dark, shadow colours. Shading follows the game's icons: light edge on top and left,
    darker edge on the bottom and right, a hard dark shadow one pixel down and to the right."""
    mask, w, h = arrow_mask()
    ox = (BUTTON_W - w) // 2 - 1
    oy = (BUTTON_H - h) // 2
    px = button.load()
    # shadow first, so that the arrow covers most of it
    for (x, y) in mask:
        for dx, dy in ((1, 0), (0, 1), (1, 1)):
            if (x + dx, y + dy) not in mask:
                px[ox + x + dx, oy + y + dy] = palette["shadow"]
    for (x, y) in mask:
        top = (x, y - 1) not in mask or (x - 1, y) not in mask
        bottom = (x, y + 1) not in mask or (x + 1, y) not in mask
        if top and not bottom:
            c = palette["light"]
        elif bottom and not top:
            c = palette["dark"]
        else:
            c = palette["base"]
        px[ox + x, oy + y] = c
    return button


def make_marker(hilite):
    crop = hilite.crop(MARKER_SRC_BOX)
    px = crop.load()
    bg = Counter(hilite.getdata()).most_common(1)[0][0]
    # make the background transparent, by flooding in from the edge of the crop
    todo = [(x, y) for x in range(crop.width) for y in (0, crop.height - 1)] + [(x, y) for y in range(crop.height) for x in (0, crop.width - 1)]
    seen = set()
    while todo:
        x, y = todo.pop()
        if (x, y) in seen or not (0 <= x < crop.width and 0 <= y < crop.height):
            continue
        seen.add((x, y))
        if px[x, y] == bg:
            px[x, y] = (0, 0, 0, 0)
            todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    # scale down with premultiplied alpha, so that the edges do not get a dark fringe
    pre = Image.new("RGBA", crop.size)
    pp = pre.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = px[x, y]
            pp[x, y] = (r * a // 255, g * a // 255, b * a // 255, a)
    small = pre.resize(MARKER_SIZE, Image.BOX)
    sp = small.load()
    for y in range(small.height):
        for x in range(small.width):
            r, g, b, a = sp[x, y]
            if a:
                sp[x, y] = (min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a), a)
    return small


def main():
    with zipfile.ZipFile(BASE_WZ) as z:
        save, saveh = load(z, "image_des_save"), load(z, "image_des_saveh")
    to_normal = colour_map(save, saveh)

    # the palette of the highlighted icon: its brightest, its main, its darkest fill and the shadow
    hi = {"light": (204, 230, 255, 255), "base": (166, 186, 220, 255), "dark": (129, 142, 184, 255), "shadow": (22, 22, 19, 255)}
    lo = {k: to_normal[v] for k, v in hi.items()}

    upgrade = paint_arrow(blank_button(save), lo)
    upgradeh = paint_arrow(blank_button(saveh), hi)
    marker = make_marker(saveh)

    atlas = Image.new("RGBA", (ATLAS, ATLAS), (0, 0, 0, 0))
    atlas.paste(upgrade, UPGRADE_POS)
    atlas.paste(upgradeh, UPGRADEH_POS)
    atlas.paste(marker, MARKER_POS)
    buf = io.BytesIO()
    atlas.save(buf, "PNG", optimize=True)
    data = buf.getvalue()

    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    sheet = Image.new("RGBA", (ATLAS * 6, ATLAS * 6), (70, 70, 70, 255))
    sheet.paste(atlas.resize((ATLAS * 6, ATLAS * 6), Image.NEAREST), (0, 0), atlas.resize((ATLAS * 6, ATLAS * 6), Image.NEAREST))
    sheet.save(PREVIEW)

    rows = []
    for i in range(0, len(data), 16):
        rows.append("\t" + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    with open(OUT_CPP, "w", newline="\n") as f:
        f.write("""/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file designvault_icondata.cpp
 * Generated by scripts/gen-icons.py, do not edit by hand. Pixel data of the three small images the design vault adds to the
 * design screen (see designvault_icons.h), as one PNG.
 */

#include "designvault_icons.h"

namespace DesignVaultIcons
{
const unsigned char atlasPng[] = {
""")
        f.write("\n".join(rows))
        f.write("""
};
const size_t atlasPngSize = sizeof(atlasPng);

const int atlasSize = %d;
const Rect upgradeRect = { %d, %d, %d, %d };
const Rect upgradeHighlightRect = { %d, %d, %d, %d };
const Rect markerRect = { %d, %d, %d, %d };
}
""" % (ATLAS, UPGRADE_POS[0], UPGRADE_POS[1], BUTTON_W, BUTTON_H, UPGRADEH_POS[0], UPGRADEH_POS[1], BUTTON_W, BUTTON_H, MARKER_POS[0], MARKER_POS[1], MARKER_SIZE[0], MARKER_SIZE[1]))
    print("wrote %s (%d bytes of PNG), preview %s" % (OUT_CPP, len(data), PREVIEW))


main()
