"""Explicit private-image input and public exact-image qualification only."""
from pathlib import Path
import hashlib, os, re

SUPPORTED_IMAGES = {
    "d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60": "retained C1 code image",
    "a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976": "shipping Kernel33-import executable",
}

def image_argument(value=None):
    value = value or os.environ.get("CODOL_NEWS_TEST_IMAGE")
    if not value:
        raise SystemExit("Supply --image PATH or CODOL_NEWS_TEST_IMAGE for an owned, supported private C1 image.")
    path = Path(value).expanduser()
    if not path.is_file():
        raise SystemExit("The explicitly supplied private image is not a regular file.")
    return path.resolve()

def load_image(value=None):
    import pefile
    data = image_argument(value).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest not in SUPPORTED_IMAGES:
        raise SystemExit("Unsupported image SHA-256; refusing to execute an unqualified image.")
    pe = pefile.PE(data=data, fast_load=True)
    if pe.FILE_HEADER.Machine != 0x14C or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
        raise SystemExit("Unsupported PE geometry; expected the pinned x86 C1 image.")
    return data, pe, pe.get_memory_mapped_image()

def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value

def guard_regions(source_root):
    source = (Path(source_root) / "news_refresh_guards.h").read_text(encoding="utf-8")
    pattern = r'\{"([^\"]+)",0x([0-9A-Fa-f]+)u,(\d+)u,0x([0-9A-Fa-f]+)u\}'
    rows = [dict(name=name, va=int(va,16), length=int(size), hash=int(value,16))
            for name,va,size,value in re.findall(pattern,source)]
    if len(rows) != 15 or sum(row["length"] for row in rows) != 2651:
        raise SystemExit("Guard geometry changed; requalify the regression fixture before use.")
    return rows
