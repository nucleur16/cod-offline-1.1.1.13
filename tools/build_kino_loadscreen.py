"""Build Kino's original Black Ops loading artwork into a CoDOL loading zone.

The material/2d technique contract comes from the pack's Factory loading zone.
Only the level-briefing image and zone marker change; image pixels are embedded
in the loading zone, avoiding a new encrypted IWD or native client dependency.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import struct
import zipfile

sys.path.insert(0, str(Path(__file__).absolute().parents[1]))
from zonekit import codecs, container, zoneimage
from PIL import Image

BO1_IMAGE_SHA256 = "32011cf36a54c98fb489a1c617bf66ef0f3e831f261dafe4604c30ec49bc3aea"
BO1_IMAGE_ENTRY = "images/loadscreen_zombie_theater.iwi"

FACTORY_SHA256 = "8131e0567194afb71f9fda5797ce5a07fc76560277439762e3bf0cdf5a85eada"  # Exact user-supplied 2026-10-31 pack.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--factory-load", type=Path, required=True)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--bo1-game", type=Path, required=True, help="Original installed Black Ops game root")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    source = args.factory_load.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != FACTORY_SHA256:
        raise ValueError("Factory loading-zone input differs from the reviewed map pack")
    unsigned = container.read_unsigned(source, args.exe)
    image = zoneimage.parse(unsigned.zone)
    assert image.flat and len(image.assets) == 4
    assert [(t, codecs.guess_name(t, b)) for t, _, b in image.assets] == [
        (9, "2d"), (5, "$defeatbackdrop"), (5, "$levelbriefing"), (36, "zm_factory_sh_load")]

    archive_path = args.bo1_game / "main/iw_04.iwd"
    with zipfile.ZipFile(archive_path) as archive:
        original = archive.read(BO1_IMAGE_ENTRY)
    assert hashlib.sha256(original).hexdigest() == BO1_IMAGE_SHA256
    raw_copy = args.bo1_game / "raw/images/loadscreen_zombie_theater.iwi"
    if raw_copy.is_file():
        assert raw_copy.read_bytes() == original
    assert original[:4] == b"IWi\x0d"
    fmt, flags, width, height, depth = struct.unpack_from("<BB3H", original, 4)
    assert (fmt, flags, width, height, depth) == (11, 195, 1024, 1024, 1)
    assert len(original) == 48 + width * height // 2
    screen = Image.frombytes("RGBA", (width, height), original[48:], "bcn", (1, "DXT1"))
    # Preserve every decoded source pixel and the authored texture dimensions.
    # The original BO1 1280x720 loading movie confirms this square texture is
    # authored for full-screen 16:9 UV mapping; do not crop or letterbox it.
    pixel_sha256 = hashlib.sha256(screen.tobytes()).hexdigest()
    assert pixel_sha256 == "84ce9946fe443055ec1e3d825aca3f15cb377ba0d905fd684fe350076e59d295"

    material = image.assets[2][2]
    # Exact reviewed Factory blob: texture-table image starts at 123, followed
    # by two state-bit dwords at 200. Assert all boundaries before replacing it.
    old_image = material[123:200]
    assert old_image[:4] == b"\xff" * 4
    assert old_image[36:].startswith(b"loadscreen_zm_factory_sh\0")
    assert len(material) == 208
    # The embedded source supplies exactly one level. Bit 1 disables mipmaps:
    # Image_CountMipmaps/Image_Setup otherwise request the complete chain.
    # Stock C1 inline 2D images use flags 3 for a single level too. Keep the
    # original no-picmip/no-mipmap bits, without BO1's unrelated high bits.
    load_flags = flags & 3
    assert load_flags == 3
    new_image = codecs.encode_image("loadscreen_zm_theater_original_bo1", [screen.tobytes("raw", "BGRA")],
                                   width, height, {"hdr": old_image[4:32].hex(), "lflags": hex(load_flags)})
    decoded_name, decoded_levels, decoded_params = codecs.decode_image(new_image)
    assert decoded_name == "loadscreen_zm_theater_original_bo1"
    assert decoded_levels == [screen.tobytes("raw", "BGRA")]
    assert int(decoded_params["lflags"], 0) == 3
    assert int(decoded_params["levels"]) == 1
    image.assets[2] = (5, image.assets[2][1], material[:123] + new_image + material[200:])
    marker, data, params = codecs.decode_rawfile(image.assets[3][2])
    assert marker == "zm_factory_sh_load" and not data
    image.assets[3] = (36, image.assets[3][1], codecs.encode_rawfile("zm_theater_sh_load", data, params))
    # Grow blocks conservatively, preserving the existing technique backrefs.
    image.blocks[0] = max(image.blocks[0], width * height * 4 + 4096)
    image.blocks[4] = max(image.blocks[4], sum(len(b) for _, _, b in image.assets) + 4096)
    zone = zoneimage.assemble(image)
    output = container.write_unsigned(zone, args.exe, filetime=unsigned.filetime, rsa_blob=unsigned.rsa_blob)
    assert container.read_unsigned(output, args.exe).zone == zone
    assert len(zoneimage.parse(zone).assets) == 4
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(output)
    screen.save(args.out.with_suffix(".texture.png"))
    # Preview the existing loading material's full-screen display convention.
    screen.resize((1280, 720), Image.Resampling.LANCZOS).convert("RGB").save(args.out.with_suffix(".png"))
    receipt = dict(factory_input_sha256=digest, output_sha256=hashlib.sha256(output).hexdigest(),
                   output_size=len(output), embedded_image=True, external_iwd_dependencies_added=0,
                   client_visual_acceptance=False, native_client_changed=False,
                   artwork="original_Black_Ops_Kino_der_Toten",
                   source_archive=str(archive_path), source_entry=BO1_IMAGE_ENTRY,
                   source_image_sha256=BO1_IMAGE_SHA256, source_image_size=len(original),
                   source_texture_dimensions=[width, height], output_texture_dimensions=[width, height],
                   decoded_pixel_sha256=pixel_sha256, decoded_pixels_preserved_exactly=True,
                   image_levels=1, image_load_flags=load_flags, mipmaps_disabled=True,
                   drawn_overlays=False, AI_generated_or_edited=False,
                   preview_display_dimensions=[1280, 720],
                   display_aspect_reference="Original main/video/zombie_theater_load.bik, 1280x720")
    args.out.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n", newline="\n")
    print(json.dumps(receipt))


if __name__ == "__main__":
    main()
