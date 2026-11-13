#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos
"""Makes the program icons of every platform from misc/logo/djw-icon.svg into misc/icons/.

    tools/make_icons.py

Needs rsvg-convert (librsvg) and ImageMagick (magick). The .icns of macOS is written here, it holds PNG images.
The icon is the record on a black square with rounded corners; each platform wants it a little differently:
Windows takes it as it is, macOS with the margin and the corners of its icon grid, iOS and the Play Store as a full
square without transparency (they round the corners themselves), Android as an adaptive icon: the record alone in
the middle (the launcher cuts the shape) on a black background.
"""

import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "misc" / "logo" / "djw-icon.svg"
TARGET = ROOT / "misc" / "icons"

# The square of the icon in the SVG: viewBox="365 60 526 526"
X, Y, SIZE = 365.0, 60.0, 526.0


def variant(svg: str, area: float, radius_share: float | None, full_square: bool = False) -> str:
    """The icon in a square of SIZE / area around it (area = the part of the picture the icon takes), with the
    corners rounded by radius_share of its side, or without the black square (radius_share None). With full_square
    the black square fills the whole picture and only the record is smaller."""
    view = SIZE / area
    margin = (view - SIZE) / 2
    svg = re.sub(r'viewBox="[^"]*"', f'viewBox="{X - margin:g} {Y - margin:g} {view:g} {view:g}"', svg, count=1)
    rect = re.search(r"<rect [^>]*/>", svg)
    if rect is None:
        sys.exit("djw-icon.svg has no <rect> as its square")
    if radius_share is None:
        return svg.replace(rect.group(0), "", 1)
    side = view if full_square else SIZE
    corner = (X - margin, Y - margin) if full_square else (X, Y)
    square = (f'<rect x="{corner[0]:g}" y="{corner[1]:g}" width="{side:g}" height="{side:g}" '
              f'rx="{side * radius_share:g}" fill="#000"/>')
    return svg.replace(rect.group(0), square, 1)


def png(svg: str, size: int, path: Path, opaque: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as file:
        file.write(svg)
        source = file.name
    subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(path), source], check=True)
    Path(source).unlink()
    if opaque:
        subprocess.run(["magick", str(path), "-background", "black", "-alpha", "remove", "-alpha", "off",
                        f"PNG24:{path}"], check=True)


def icns(svg: str, path: Path, work: Path) -> None:
    """Apple's icon file: 'icns', its length, then entries of type, length and PNG data"""
    entries = [("icp4", 16), ("icp5", 32), ("ic11", 32), ("ic12", 64), ("ic07", 128), ("ic13", 256), ("ic08", 256),
               ("ic14", 512), ("ic09", 512), ("ic10", 1024)]
    data = b""
    for kind, size in entries:
        image = work / f"macos-{size}.png"
        if not image.exists():
            png(svg, size, image)
        content = image.read_bytes()
        data += kind.encode() + struct.pack(">I", 8 + len(content)) + content
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"icns" + struct.pack(">I", 8 + len(data)) + data)


def android(svg: str) -> None:
    res = TARGET / "android" / "res"
    densities = {"mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}
    # Adaptive icon: 108 dp, of which a circle of 66 dp in the middle is always shown; the record takes 60 % of it,
    # so its corners stay inside that circle
    foreground = variant(svg, 0.6, None)
    # Before Android 8: 48 dp, the icon as it is, round where the launcher wants it round
    legacy = variant(svg, 1.0, 0.1808)
    round_legacy = variant(svg, 0.8, 0.5, full_square=True)
    for density, scale in densities.items():
        png(foreground, round(108 * scale), res / f"mipmap-{density}" / "ic_launcher_foreground.png")
        png(legacy, round(48 * scale), res / f"mipmap-{density}" / "ic_launcher.png")
        png(round_legacy, round(48 * scale), res / f"mipmap-{density}" / "ic_launcher_round.png")
    adaptive = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
    <monochrome android:drawable="@mipmap/ic_launcher_foreground"/>
</adaptive-icon>
"""
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        (res / "mipmap-anydpi-v26").mkdir(parents=True, exist_ok=True)
        (res / "mipmap-anydpi-v26" / name).write_text(adaptive)
    (res / "values").mkdir(parents=True, exist_ok=True)
    (res / "values" / "ic_launcher_background.xml").write_text("""<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="ic_launcher_background">#000000</color>
</resources>
""")
    # The Play Store: 512 px, a full square without transparency
    png(variant(svg, 1.0, 0.0), 512, TARGET / "android" / "play-store-512.png", opaque=True)


def main() -> None:
    for tool in ("rsvg-convert", "magick"):
        if shutil.which(tool) is None:
            sys.exit(f"{tool} is needed")
    svg = SOURCE.read_text()
    for platform in ("windows", "macos", "ios", "android"):  # the README stays
        shutil.rmtree(TARGET / platform, ignore_errors=True)
    with tempfile.TemporaryDirectory() as temp:
        work = Path(temp)

        # Windows: one .ico with every size Explorer and the task bar use
        sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
        windows = variant(svg, 1.0, 0.1808)
        images = []
        for size in sizes:
            image = work / f"windows-{size}.png"
            png(windows, size, image)
            images.append(str(image))
        (TARGET / "windows").mkdir(parents=True)
        subprocess.run(["magick", *images, str(TARGET / "windows" / "djw.ico")], check=True)

        # macOS: the icon grid of Big Sur and later: 824 of 1024 px, corners of 22.4 %
        icns(variant(svg, 824 / 1024, 0.224), TARGET / "macos" / "djw.icns", work)

        # iOS: one 1024 px image, a full square without transparency; Xcode makes the other sizes
        png(variant(svg, 1.0, 0.0), 1024, TARGET / "ios" / "AppIcon-1024.png", opaque=True)

        android(svg)
    print(f"icons written to {TARGET.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
