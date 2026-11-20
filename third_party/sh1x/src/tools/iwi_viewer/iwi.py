"""Read-only decoder for Call of Duty Online version 9 IWI textures.

Layout verified against C1 files; format/mip semantics cross-checked against
OpenBLOPS Engine/Renderer/gfx_d3d/r_image_load_obj.cpp.
"""
import struct
from pathlib import Path
from PIL import Image, ImageOps

DEFAULT_ROOT = r'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\main\_decrypted'
FORMATS = {1: 'BGRA8', 2: 'BGR8', 3: 'LA8', 4: 'L8', 5: 'A8', 11: 'DXT1', 12: 'DXT3', 13: 'DXT5'}
IMAGE_EXTENSIONS = {'.iwi', '.png', '.jpg', '.jpeg', '.jfif', '.dds', '.tga', '.bmp', '.dib', '.gif', '.webp', '.tif', '.tiff', '.ico', '.pcx', '.ppm', '.pgm', '.pbm'}


def scan_images(root):
    """Index every supported image below root without decoding the entire tree."""
    import os
    if not root.exists():
        raise ValueError(f'Folder does not exist: {root}')
    if root.is_file():
        return [(root, root.name)]
    items = []
    for folder, _, files in os.walk(root):
        for name in files:
            if Path(name).suffix.lower() in IMAGE_EXTENSIONS:
                p = Path(folder) / name
                items.append((p, str(p.relative_to(root))))
    return sorted(items, key=lambda x: x[1].lower())


def decode_image(path, face=0):
    if path.suffix.lower() == '.iwi':
        return decode(path, face)
    with Image.open(path) as source:
        fmt = source.format
        image = ImageOps.exif_transpose(source).convert('RGBA')
    return image, dict(width=image.width, height=image.height, faces=1,
                       format=fmt, bytes=path.stat().st_size)


def header(data):
    if len(data) < 32 or data[:4] != b'IWi\x09':
        raise ValueError('Expected a CoDOL version 9 IWI header')
    flags, = struct.unpack_from('<I', data, 4)
    fmt, width, height, depth = struct.unpack_from('<4H', data, 8)
    if not width or not height or width * height > 67108864:
        raise ValueError('Invalid or oversized texture dimensions')
    return dict(width=width, height=height, depth=depth, flags=flags,
                format=FORMATS.get(fmt, f'Unsupported format {fmt}'), format_id=fmt)


def decode(path, face=0):
    data = path.read_bytes()
    meta = header(data)
    w, h, fmt = meta['width'], meta['height'], meta['format_id']
    faces = 6 if meta['flags'] & 4 else max(1, meta['depth'])
    if not 0 <= face < faces:
        raise ValueError('Face/slice outside this texture')
    if fmt in (11, 12, 13):
        size = ((w + 3) // 4) * ((h + 3) // 4) * (8 if fmt == 11 else 16)
    elif fmt in (1, 2, 3, 4, 5):
        size = w * h * {1: 4, 2: 3, 3: 2, 4: 1, 5: 1}[fmt]
    else:
        raise ValueError(f'IWI format {fmt} is not supported')
    # IWI levels are smallest first, all faces/slices within each mip level.
    offset = len(data) - size * faces + face * size
    if offset < 32:
        raise ValueError('Texture payload is truncated or uses an unsupported layout')
    raw = data[offset:offset + size]
    if fmt in (11, 12, 13):
        img = Image.frombytes('RGBA', (w, h), raw, 'bcn', ({11: 1, 12: 2, 13: 3}[fmt], FORMATS[fmt]))
    elif fmt == 5:
        img = Image.new('RGBA', (w, h), 'white')
        img.putalpha(Image.frombytes('L', (w, h), raw))
    else:
        mode, rawmode = {1: ('RGBA', 'BGRA'), 2: ('RGB', 'BGR'), 3: ('LA', 'LA'), 4: ('L', 'L')}[fmt]
        img = Image.frombytes(mode, (w, h), raw, 'raw', rawmode).convert('RGBA')
    meta.update(faces=faces, bytes=len(data))
    return img, meta
