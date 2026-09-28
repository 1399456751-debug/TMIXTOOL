"""Generates the application icon.

Kept as a script rather than a checked-in binary so the artwork can be
regenerated and adjusted: proportions, colour and corner radius are all here.

WHY NOT THE DRAWING
-------------------
The project's line-art drawing was measured at icon sizes and does not survive:
five crops, four binarise-and-thicken settings, twelve regions of the image and
three contrast treatments were all tried, and every one of them was
unstructured noise at 32 px. The scene simply contains too many separate
elements. So the icon is a mark instead - a geometric T, drawn from rectangles
rather than set in a font so the proportions are exact at every size - in the
drawing's own ink colour.

The solid variant was chosen because it is the only one still unambiguously
readable at 16 px, which is the size a taskbar actually renders.

Output: src/gui/assets/tmixtool.ico and tmixtool.png
"""

import os
from PIL import Image, ImageDraw

# Sampled from the artwork, so the icon stays in its palette.
INK   = (108, 46, 62)
PAPER = (252, 248, 249)

# Drawn large and downscaled, which is what gives clean edges.
SUPERSAMPLE = 1024

ICO_SIZES = [256, 128, 64, 48, 32, 24, 16]


def draw_mark(d, cx, cy, w, h, colour):
    """A geometric T: a crossbar and a stem, drawn from two rectangles."""
    bar_height = h * 0.255
    stem_width = w * 0.235
    x0 = cx - w / 2
    y0 = cy - h / 2

    d.rectangle([x0, y0, x0 + w, y0 + bar_height], fill=colour)
    d.rectangle([cx - stem_width / 2, y0, cx + stem_width / 2, y0 + h], fill=colour)


def build_master():
    im = Image.new("RGB", (SUPERSAMPLE, SUPERSAMPLE), PAPER)
    d = ImageDraw.Draw(im)

    d.rounded_rectangle(
        [0, 0, SUPERSAMPLE - 1, SUPERSAMPLE - 1],
        radius=int(SUPERSAMPLE * 0.185),
        fill=INK,
    )
    draw_mark(d, SUPERSAMPLE / 2, SUPERSAMPLE / 2,
              SUPERSAMPLE * 0.50, SUPERSAMPLE * 0.50, PAPER)

    return im


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    outdir = os.path.join(root, "src", "gui", "assets")
    os.makedirs(outdir, exist_ok=True)

    master = build_master()

    ico_path = os.path.join(outdir, "tmixtool.ico")
    master.save(ico_path, format="ICO", sizes=[(s, s) for s in ICO_SIZES])
    print(f"ico  -> {ico_path}")

    # A plain PNG as well: handy for the README, and for any tooling that
    # cannot read .ico.
    png_path = os.path.join(outdir, "tmixtool.png")
    master.resize((256, 256), Image.LANCZOS).save(png_path)
    print(f"png  -> {png_path}")

    size = os.path.getsize(ico_path)
    print(f"icon sizes {ICO_SIZES}, {size / 1024:.1f} KB")


if __name__ == "__main__":
    main()
