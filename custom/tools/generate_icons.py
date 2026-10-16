#!/usr/bin/env python3
"""Regenerate TagTracker platform icons from custom/res/icons/tagtracker.svg.

Requires rsvg-convert (librsvg) and, for the macOS .icns, iconutil (macOS only).

Not generated here (hand-maintained copies of the artwork, update them manually):
res/icons/tagtracker_logo*.svg and android/res/drawable/ic_launcher_foreground.xml.
"""

from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

CUSTOM_DIR = Path(__file__).resolve().parent.parent
ICONS_DIR = CUSTOM_DIR / "res" / "icons"
SOURCE_SVG = ICONS_DIR / "tagtracker.svg"
SVG_NS = "http://www.w3.org/2000/svg"

ANDROID_SPLASH_SIZES = {
    "ldpi": 36,
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
ICNS_SIZES = (16, 32, 128, 256, 512)

# Apple's macOS icon grid: a rounded-rect body centered on a square canvas; macOS doesn't mask icons.
MACOS_CANVAS = 1024
MACOS_BODY = 824
MACOS_RADIUS = 185


def render_png(svg: Path, size: int, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(out), str(svg)],
        check=True,
    )


def write_macos_svg(out: Path) -> None:
    ET.register_namespace("", SVG_NS)
    inset = str((MACOS_CANVAS - MACOS_BODY) // 2)
    body = str(MACOS_BODY)
    canvas = str(MACOS_CANVAS)

    source = ET.parse(SOURCE_SVG).getroot()
    source.attrib.update({"x": inset, "y": inset, "width": body, "height": body})

    root = ET.Element(
        f"{{{SVG_NS}}}svg",
        {"viewBox": f"0 0 {canvas} {canvas}", "width": canvas, "height": canvas},
    )
    clip = ET.SubElement(root, f"{{{SVG_NS}}}clipPath", {"id": "macosBody"})
    ET.SubElement(
        clip,
        f"{{{SVG_NS}}}rect",
        {"x": inset, "y": inset, "width": body, "height": body, "rx": str(MACOS_RADIUS)},
    )
    group = ET.SubElement(root, f"{{{SVG_NS}}}g", {"clip-path": "url(#macosBody)"})
    group.append(source)
    ET.ElementTree(root).write(out)


def write_ico(pngs: list[tuple[int, bytes]], out: Path) -> None:
    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = len(header) + 16 * len(pngs)
    entries = b""
    for size, data in pngs:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    out.write_bytes(header + entries + b"".join(data for _, data in pngs))


def main() -> int:
    if not shutil.which("rsvg-convert"):
        print(
            "rsvg-convert (from librsvg) not found on PATH. Install it, e.g.:\n"
            "  macOS:         brew install librsvg\n"
            "  Debian/Ubuntu: sudo apt install librsvg2-bin\n"
            "  Fedora:        sudo dnf install librsvg2-tools\n"
            "  Windows:       MSYS2 pacman -S mingw-w64-ucrt-x86_64-librsvg",
            file=sys.stderr,
        )
        return 1

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)

        render_png(SOURCE_SVG, 256, ICONS_DIR / "tagtracker_256.png")

        for density, size in ANDROID_SPLASH_SIZES.items():
            render_png(
                SOURCE_SVG,
                size,
                CUSTOM_DIR / "android" / "res" / f"drawable-{density}" / "icon.png",
            )

        ico_pngs = []
        for size in ICO_SIZES:
            png = tmp / f"ico_{size}.png"
            render_png(SOURCE_SVG, size, png)
            ico_pngs.append((size, png.read_bytes()))
        write_ico(ico_pngs, ICONS_DIR / "tagtracker.ico")

        if shutil.which("iconutil"):
            macos_svg = tmp / "macos.svg"
            write_macos_svg(macos_svg)
            iconset = tmp / "tagtracker.iconset"
            for size in ICNS_SIZES:
                render_png(macos_svg, size, iconset / f"icon_{size}x{size}.png")
                render_png(macos_svg, size * 2, iconset / f"icon_{size}x{size}@2x.png")
            subprocess.run(
                ["iconutil", "-c", "icns", "-o", str(ICONS_DIR / "tagtracker.icns"), str(iconset)],
                check=True,
            )
        else:
            print("iconutil not found; skipping tagtracker.icns", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
