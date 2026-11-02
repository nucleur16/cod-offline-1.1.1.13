"""Bounded C1 diagnostic: complete T5 split-hand idle with C1 opposite-arm pose.

This preserves the active-hand animation and copies missing opposite-arm tracks
from a specified original C1 static dual idle. It is an asset adaptation experiment,
not proof of original animation parity. No installation or executable launch.
"""
from __future__ import annotations

from dataclasses import dataclass, replace
import re
import struct


class Unsupported(ValueError):
    pass


COUNTS = (4, 6, 8, 32, 10, 12)
WIDTHS = (1, 2, 4, 2, 1, 4)
PATCH_SHA256 = "30791750a760cd1c6f6ce9042babdf8274747d8b236ab3bc98a7c20a4a71445a"
COMMON_SHA256 = "3aa06930259b98a4bdf33c1e2176f1f2436435f9374d58478aa8067b682c6991"
STATE_NAMES = tuple(f"viewmodel_colt1911_dw_{side}_{state}" for state in
                    ("idle_loop", "fire", "last_shot", "reload_empty") for side in ("right", "left"))


@dataclass(frozen=True)
class Clip:
    header: bytes
    name: str
    bones: tuple[str, ...]
    notifies: tuple[tuple[str, bytes], ...]
    streams: tuple[bytes, ...]
    indices: bytes


def u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def parse(blob: bytes, strings: list[str | None]) -> Clip:
    """Read the exact 88-byte C1 representation; reject pooled/delta data."""
    if len(blob) < 88 or u32(blob, 0) != 0xffffffff:
        raise Unsupported("requires inline XAnim name")
    if u32(blob, 84):
        raise Unsupported("diagnostic supports no delta")
    end = blob.index(b"\0", 88)
    name = blob[88:end].decode("ascii")
    pos = end + 1
    count = blob[26]
    if u32(blob, 48) != 0xffffffff:
        raise Unsupported("requires inline bone script strings")
    ids = struct.unpack_from(f"<{count}H", blob, pos)
    if any(i >= len(strings) or not strings[i] for i in ids):
        raise Unsupported("invalid local script-string index")
    bones = tuple(strings[i] for i in ids)
    if len(set(bones)) != count or sum(blob[17:22]) != count or sum(blob[22:26]) != count:
        raise Unsupported("duplicate bones or inconsistent rotation/translation groups")
    pos += count * 2
    notifies = []
    if u32(blob, 80) != (0xffffffff if blob[27] else 0):
        raise Unsupported("requires inline notifies")
    for _ in range(blob[27]):
        event = blob[pos:pos+8]
        if len(event) != 8 or u16(event, 0) >= len(strings) or not strings[u16(event, 0)]:
            raise Unsupported("invalid notify or local notify script string")
        notifies.append((strings[u16(event, 0)], event[2:]))
        pos += 8
    streams = []
    for i, (offset, width) in enumerate(zip(COUNTS, WIDTHS)):
        n = u32(blob, offset) if i == 3 else u16(blob, offset)
        ptr = u32(blob, 52 + i * 4)
        if ptr != (0xffffffff if n else 0):
            raise Unsupported("requires inline stream or zero-count null stream")
        data = blob[pos:pos+n*width]
        if len(data) != n*width:
            raise Unsupported("truncated animation stream")
        streams.append(data)
        pos += len(data)
    count = u32(blob, 36)
    width = 1 if u16(blob, 14) < 256 else 2
    if u32(blob, 76) != (0xffffffff if count else 0):
        raise Unsupported("requires inline time indices")
    indices = blob[pos:pos+count*width]
    if len(indices) != count*width or pos+len(indices) != len(blob):
        raise Unsupported("truncated indices or trailing animation bytes")
    return Clip(blob[:88], name, bones, tuple(notifies), tuple(streams), indices)


def encode(clip: Clip, strings: list[str | None]) -> bytes:
    ids = {s: i for i, s in enumerate(strings) if s is not None}
    if any(s not in ids for s in clip.bones) or any(s not in ids for s, _ in clip.notifies):
        raise Unsupported("output script-string table omits a bone")
    return (clip.header + clip.name.encode("ascii") + b"\0" +
            struct.pack(f"<{len(clip.bones)}H", *(ids[s] for s in clip.bones)) +
            b"".join(struct.pack("<H", ids[s])+tail for s, tail in clip.notifies) +
            b"".join(clip.streams) + clip.indices)


@dataclass(frozen=True)
class Layout:
    rotation_static_start: int
    rotation_static_end: int
    rotation_key_records: tuple[bytes, ...]
    translation_bone_positions: tuple[int, ...]
    translation_static_start: int
    translation_static_end: int


def layout(clip: Clip) -> Layout:
    """Validate every cursor in the measured dynamic4/static4 C1 representation.

    Each dynamic curve has a dataShort key-count header. Time keys are dataByte
    for <256 frames, otherwise dataShort. Static4 quaternions follow rotation
    headers in dataShort. Translation records then consume one bone-index byte,
    a key header and time keys. Normal samples use randomDataByte; precise
    samples use randomDataShort. Both have six dataInt float bounds.
    Separate seek indices / dynamic2 / static2 are outside these eight sources.
    """
    h = clip.header
    if h[18] or h[20]:
        raise Unsupported("dynamic2/static2 rotation not supported")
    if clip.indices or u32(h, 36) or clip.streams[5]:
        raise Unsupported("separate seek indices/random integers not supported")
    byte_at = short_at = random_shorts = random_bytes = 0
    width = 1 if u16(h, 14) < 256 else 2

    def times():
        nonlocal byte_at, short_at
        if short_at+2 > len(clip.streams[1]):
            raise Unsupported("truncated dynamic key-count header")
        head = clip.streams[1][short_at:short_at+2]
        last = u16(head, 0)
        short_at += 2
        if width == 2:
            if last > 63:
                raise Unsupported("separate 16-bit time seek table not supported")
            raw = clip.streams[1][short_at:short_at+(last+1)*2]
            short_at += len(raw)
            values = struct.unpack(f"<{len(raw)//2}H", raw) if len(raw)%2 == 0 else ()
        else:
            raw = clip.streams[0][byte_at:byte_at+last+1]
            byte_at += len(raw)
            values = tuple(raw)
        if (len(values) != last+1 or not values or values[0] != 0 or values[-1] > u16(h, 14) or
                any(a >= b for a, b in zip(values, values[1:]))):
            raise Unsupported("truncated/nonmonotonic/out-of-range time keys")
        return last+1, head+raw

    records = []
    for _ in range(h[19]):
        count, raw = times()
        records.append(raw)
        random_shorts += count*4
    static_start = short_at
    short_at += h[21]*8
    static_end = short_at
    if short_at > len(clip.streams[1]):
        raise Unsupported("truncated static quaternion payload")
    positions = []
    for precise, count in ((False, h[22]), (True, h[23])):
        for _ in range(count):
            positions.append(byte_at)
            byte_at += 1
            count, _ = times()
            if precise:
                random_shorts += count*3
            else:
                random_bytes += count*3
    translation_start = byte_at
    translation_end = byte_at+h[24]
    positions.extend(range(byte_at, byte_at+h[24]+h[25]))
    byte_at += h[24]+h[25]
    if (byte_at != len(clip.streams[0]) or short_at != len(clip.streams[1]) or
            random_shorts*2 != len(clip.streams[3]) or random_bytes != len(clip.streams[4]) or
            len(clip.streams[2]) != (h[22]+h[23])*24+h[24]*12):
        raise Unsupported("animation stream cursors do not consume exactly")
    if sorted(clip.streams[0][i] for i in positions) != list(range(len(clip.bones))):
        raise Unsupported("invalid translation bone-index permutation")
    return Layout(static_start, static_end, tuple(records), tuple(positions), translation_start, translation_end)


def rotation_layout(clip: Clip):
    value = layout(clip)
    return value.rotation_static_start, value.rotation_static_end, value.rotation_key_records


def static_pose(clip: Clip):
    h = clip.header
    if h[18] or h[19] or h[20] or h[22] or h[23] or clip.indices:
        raise Unsupported("donor must be entirely static")
    start, end, _ = rotation_layout(clip)
    if start or end != len(clip.streams[1]) or clip.streams[3] or clip.streams[4] or clip.streams[5]:
        raise Unsupported("unexpected static donor streams")
    rotations = {}
    for i, name in enumerate(clip.bones):
        rotations[name] = (struct.pack("<4h", 0, 0, 0, 32767) if i < h[17]
                           else clip.streams[1][(i-h[17])*8:(i-h[17]+1)*8])
    order = list(clip.streams[0])
    if sorted(order) != list(range(len(clip.bones))) or len(clip.streams[2]) != h[24]*12:
        raise Unsupported("invalid static translation permutation")
    translations = {}
    for i, bone in enumerate(order):
        translations[clip.bones[bone]] = (clip.streams[2][i*12:(i+1)*12]
                                         if i < h[24] else b"\0"*12)
    return rotations, translations


def complete(clip: Clip, donor: Clip, opposite_side: str, *, all_states=False) -> tuple[Clip, tuple[str, ...]]:
    if opposite_side not in ("le", "ri"):
        raise ValueError("opposite_side must be le or ri")
    if ((all_states and clip.name not in STATE_NAMES) or
            (not all_states and not clip.name.endswith("idle_loop"))):
        raise Unsupported("non-idle completion requires one of the eight measured states")
    h = clip.header
    channels = layout(clip)
    rotation_end = channels.rotation_static_end
    rotations, translations = static_pose(donor)
    side = re.compile(r"_" + opposite_side + r"(?:_|$)")
    added = tuple(n for n in donor.bones if side.search(n) and n not in clip.bones)
    if not added or len(clip.bones)+len(added) > 253:
        raise Unsupported("no missing opposite arm or excessive bone count")
    # Normal translated-bone records occur before static translations. Their
    # six-float quantization bounds and all compressed samples remain unchanged.
    if len(clip.streams[2]) != (h[22]+h[23])*24+h[24]*12:
        raise Unsupported("unexpected translation float stream")
    byte_split = channels.translation_static_end
    streams = list(clip.streams)
    streams[0] = (streams[0][:byte_split] + bytes(range(len(clip.bones), len(clip.bones)+len(added))) +
                  streams[0][byte_split:])
    streams[1] = streams[1][:rotation_end]+b"".join(rotations[n] for n in added)+streams[1][rotation_end:]
    streams[2] += b"".join(translations[n] for n in added)
    header = bytearray(h)
    header[21] += len(added)
    header[24] += len(added)
    header[26] += len(added)
    for i, (offset, width) in enumerate(zip(COUNTS, WIDTHS)):
        count = len(streams[i])//width
        if i != 3 and count > 65535:
            raise Unsupported("stream count exceeds C1 field")
        struct.pack_into("<I" if i == 3 else "<H", header, offset, count)
    result = replace(clip, header=bytes(header), bones=clip.bones+added, streams=tuple(streams))
    # Added static rotation records must leave the dynamic keys and the entire
    # pre-existing quaternion stream intact. Both idle periods stay independent.
    assert rotation_layout(result)[2] == rotation_layout(clip)[2]
    assert result.indices == clip.indices and result.streams[3:] == clip.streams[3:]
    return result, added


def override_zone(clips: tuple[Clip, ...]) -> bytes:
    """All-inline standalone override; no existing C# pooled offsets are moved.

    Allocation cursor follows C1 XAnim loader 4b7a50, replayed in
    codol_load_blocks.c:481 and CODOL_BeginZoneReplay. Block0 is temporary
    (rewound after each asset). Block3 holds local strings and inline arrays.
    Page rounding follows the retained zone allocation boundary. There are no
    external resources; the extra header word and unused blocks are zero.
    """
    from .zoneimage import ZoneImage, assemble
    if not clips or len({c.name for c in clips}) != len(clips):
        raise Unsupported("override requires unique animation names")
    strings = [None] + list(dict.fromkeys(s for c in clips for s in
                                        c.bones+tuple(n for n, _ in c.notifies)))
    if len(strings) > 65536:
        raise Unsupported("local script strings exceed C1 field")
    blobs = tuple(encode(c, strings) for c in clips)
    at = 0

    def reserve(length, alignment):
        nonlocal at
        at = (at+alignment-1) & -alignment
        at += length

    reserve(len(strings)*4, 4)
    for name in strings:
        if name is not None:
            reserve(len(name.encode("latin-1"))+1, 1)
    reserve(len(clips)*16, 4)
    for c in clips:
        reserve(len(c.name.encode("ascii"))+1, 1)
        reserve(len(c.bones)*2, 2)
        if c.notifies:
            reserve(len(c.notifies)*8, 4)
        for stream, width in zip(c.streams, WIDTHS):
            if stream:
                reserve(len(stream), width)
        if c.indices:
            reserve(len(c.indices), 1 if u16(c.header, 14) < 256 else 2)
    page = lambda size: (size+4095) & -4096
    blocks = [0, page(88), 0, 0, page(at), 0, 0, 0, 0]
    img = ZoneImage(blocks, [0 if s is None else 0xffffffff for s in strings],
                    strings, len(strings), 0xffffffff, 0xffffffff,
                    [(2, 0xffffffff, blob) for blob in blobs])
    return assemble(img)


def main():
    """Reproducible private data-only candidate; output path is never installed."""
    import argparse
    import hashlib
    import json
    from pathlib import Path
    from . import container, zoneimage
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--patch-ff", required=True)
    ap.add_argument("--common-ff", required=True)
    ap.add_argument("--exe", required=True, help="read C1 encryption key only; never launched")
    ap.add_argument("--out", required=True)
    ap.add_argument("--states", choices=("idle", "all"), default="idle",
                    help="all is opt-in and restricted to the eight measured source clips")
    args = ap.parse_args()
    output = Path(args.out)
    if output.exists():
        raise SystemExit("refusing to replace an existing candidate")
    patch_bytes = Path(args.patch_ff).read_bytes()
    common_bytes = Path(args.common_ff).read_bytes()
    sha = lambda b: hashlib.sha256(b).hexdigest()
    if sha(patch_bytes) != PATCH_SHA256 or sha(common_bytes) != COMMON_SHA256:
        raise Unsupported("source FF differs from the exact reviewed custom/stock inputs")
    envelope = container.read_unsigned(patch_bytes, args.exe)
    patch = zoneimage.parse(envelope.zone)
    common = zoneimage.parse(container.read_any(args.common_ff, args.exe))
    result, details = [], []
    for index in range(754, 756 if args.states == "idle" else 762):
        donor_index, side = (2805, "le") if index%2 == 0 else (2806, "ri")
        if patch.assets[index][0] != 2 or common.assets[donor_index][0] != 2:
            raise Unsupported("unexpected asset type at measured source index")
        raw, donor_raw = patch.assets[index][2], common.assets[donor_index][2]
        old = parse(raw, patch.strings)
        donor = parse(donor_raw, common.strings)
        expected = "right" if side == "le" else "left"
        if old.name != STATE_NAMES[index-754] or donor.name != f"viewmodel_beretta_akimbo_{expected[0]}_idle":
            raise Unsupported("measured source clip name changed")
        new, added = complete(old, donor, side, all_states=args.states == "all")
        result.append(new)
        details.append(dict(name=old.name, source_index=index, donor_index=donor_index,
                            source_sha256=sha(raw), donor_sha256=sha(donor_raw),
                            original_bones=len(old.bones), completed_bones=len(new.bones),
                            added_original_c1_tracks=added))
    zone = override_zone(tuple(result))
    reopened = zoneimage.parse(zone)
    for (_, _, raw), expected in zip(reopened.assets, result):
        if parse(raw, reopened.strings) != expected:
            raise Unsupported("candidate animation round trip differs")
    ff = container.write_unsigned(zone, args.exe, filetime=envelope.filetime, rsa_blob=envelope.rsa_blob)
    if container.read_unsigned(ff, args.exe).zone != zone:
        raise Unsupported("candidate container round trip differs")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(ff)
    report = dict(status=f"diagnostic only; no game launch or install; states={args.states}",
                  output=str(output), ff_sha256=sha(ff), zone_sha256=sha(zone),
                  patch_ff_sha256=sha(patch_bytes), common_ff_sha256=sha(Path(args.common_ff).read_bytes()),
                  module_sha256=sha(Path(__file__).read_bytes()), assets=details,
                  blocks=reopened.blocks,
                  acceptance_pending="real C1 LuBu plus Cowboy, original binding and non-idle follow-up")
    output.with_suffix(".json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
