#!/usr/bin/env python3
"""Makes the title's home-screen art from one square image.

    make-title-art.py <source.png> <sce_sys directory>

writes icon0.png (512x512 PNG) and pic0.dds / pic1.dds, the selection and launch backgrounds (3840x2160 BC7_UNORM
DX10 DDS without mipmaps, the format the native app template validates). The backgrounds show the art centred
over a blurred, darkened copy of itself. Needs Pillow and etcpak (installed into deps/venv by scripts/setup.sh).
"""
import struct
import sys
from pathlib import Path

import etcpak
from PIL import Image, ImageEnhance, ImageFilter

WIDTH, HEIGHT = 3840, 2160
DXGI_FORMAT_BC7_UNORM = 98
D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3


def background(art: Image.Image) -> Image.Image:
    # Fill: the art scaled to cover the screen, blurred and darkened.
    cover = max(WIDTH / art.width, HEIGHT / art.height)
    fill = art.convert("RGB").resize((round(art.width * cover), round(art.height * cover)), Image.LANCZOS)
    left, top = (fill.width - WIDTH) // 2, (fill.height - HEIGHT) // 2
    fill = fill.crop((left, top, left + WIDTH, top + HEIGHT)).filter(ImageFilter.GaussianBlur(60))
    fill = ImageEnhance.Brightness(fill).enhance(0.35).convert("RGBA")
    # Foreground: the art itself, as tall as most of the screen.
    size = round(HEIGHT * 0.8)
    front = art.resize((size, size), Image.LANCZOS)
    fill.alpha_composite(front, ((WIDTH - size) // 2, (HEIGHT - size) // 2))
    return fill


def bc7_dds(image: Image.Image) -> bytes:
    width, height = image.size
    data = etcpak.compress_bc7(image.convert("RGBA").tobytes(), width, height, None)
    linear_size = width // 4 * (height // 4) * 16
    assert len(data) == linear_size
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000  # caps, height, width, pixel format, mipmap count, linear size
    header = struct.pack("<4sIIIIIII44x", b"DDS ", 124, flags, height, width, linear_size, 0, 1)
    pixel_format = struct.pack("<II4s20x", 32, 0x4, b"DX10")  # DDPF_FOURCC
    caps = struct.pack("<IIII4x", 0x1000, 0, 0, 0)  # DDSCAPS_TEXTURE
    dx10 = struct.pack("<IIIII", DXGI_FORMAT_BC7_UNORM, D3D10_RESOURCE_DIMENSION_TEXTURE2D, 0, 1, 0)
    return header + pixel_format + caps + dx10 + data


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, target = Path(sys.argv[1]), Path(sys.argv[2])
    art = Image.open(source).convert("RGBA")
    if art.width != art.height:
        sys.exit(f"{source} must be square, it is {art.width}x{art.height}")
    target.mkdir(parents=True, exist_ok=True)
    art.resize((512, 512), Image.LANCZOS).save(target / "icon0.png", optimize=True)
    dds = bc7_dds(background(art))
    (target / "pic0.dds").write_bytes(dds)
    (target / "pic1.dds").write_bytes(dds)
    print(f"Title art from {source}: icon0.png, pic0.dds, pic1.dds in {target}")


if __name__ == "__main__":
    main()
