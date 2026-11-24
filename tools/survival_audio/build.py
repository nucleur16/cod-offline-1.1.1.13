# -*- coding: utf-8 -*-
r"""Survival's sound data: the alias blobs of zones/patch_common_gametype_survival, and the optional audio pack.

    python tools/survival_audio/build.py zone
        writes zones/patch_common_gametype_survival/sound/*.bin (aliases.py BASE: files every install streams) and
        prints the manifest lines for zone_source/patch_common_gametype_survival.csv.

    python tools/survival_audio/build.py pack --survival <2.6.9.7 folder> --lines <3.5.6.2 folder> --c1 <C1.exe>
                                              --dumper <CodolSoundDumper.exe> [--out build/survival_audio]
        builds the two files that go onto a 1.1.1.13 install (neither is in git: they hold CoD Online's audio):
            main\iw_sound_22.iwd   2.6.9.7's Survival announcer lines and music
            client_shipRetail\chinese\patch_localized_common_gametype_survival.ff   the aliases over them, and their
                                   subtitles (Chinese; the english zone has the same keys in English)
        C1's survival category loads that zone when it is there (src/rooms/survival_category_policy.h) and only prints
        when it is not; maps\mp\gametypes\_survival_audio asks soundExists() and falls back to 1.1.1.13's own lines.

Sources: both folders are CoD Online installs the user has. --dumper is the CODOL AssetExplorer's command line tool.
iw_sound_22 is free in 1.1.1.13, the map pack and the ported maps (they use 00..21); C1 opens 00..23."""
from __future__ import annotations

import argparse
import csv
import os
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path[:0] = [ROOT, HERE]

import aliases  # noqa: E402
from zonekit import codecs, container, zoneimage  # noqa: E402

ZONE_DIR = os.path.join(ROOT, "zones", "patch_common_gametype_survival")
PACK_ZONE = "patch_localized_common_gametype_survival"
PACK_IWD = "iw_sound_22.iwd"
IWD_KEY_BLOB = 0x838B20          # g_iwdAesKeyRsaBlob 0x00C39B20 in C1.exe, file offset
MARGIN = 0x10000


def zone():
    out = os.path.join(ZONE_DIR, "sound")
    os.makedirs(out, exist_ok=True)
    for name, variants in aliases.BASE.items():
        with open(os.path.join(out, name + ".bin"), "wb") as f:
            f.write(aliases.blob(name, variants))
        print("sound,%s,file=sound/%s.bin" % (name, name))


# ---- the IWD: a zip whose names and data are AES-192-CTR encrypted (CODOL-AssetExplorer Iwd/IwdWriter.cs) -----------
def iwd_key(c1):
    with open(c1, "rb") as f:
        f.seek(IWD_KEY_BLOB)
        blob = f.read(256)
    from Crypto.Cipher import PKCS1_OAEP
    from Crypto.Hash import SHA256
    key = PKCS1_OAEP.new(container.rsa_key(), hashAlgo=SHA256).decrypt(blob)
    if len(key) != 24:
        raise SystemExit("%s: the IWD key blob decrypts to %d bytes" % (c1, len(key)))
    return key


def ctr_be(key, iv, data):
    from Crypto.Cipher import AES
    from Crypto.Util import Counter
    return AES.new(key, AES.MODE_CTR, counter=Counter.new(128, initial_value=int.from_bytes(iv, "big"))).encrypt(data)


FILLER = 0x01010101


def write_iwd(path, entries, key):
    """`entries`: [(game path, bytes)]."""
    crc_name = zlib.crc32(os.path.basename(path).encode("ascii"))
    body, central = bytearray(), bytearray()
    for i, (name, data) in enumerate(entries):
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        stored, method = c.compress(data) + c.flush(), 8
        if len(stored) >= len(data):
            stored, method = data, 0
        raw = name.replace("\\", "/").encode("ascii")
        off = len(body)
        pos = off + 30 + len(raw)
        enc_name = ctr_be(key, struct.pack("<4I", FILLER, crc_name, i, FILLER), raw)
        enc = b"".join(ctr_be(key, struct.pack("<4I", pos + k, crc_name, FILLER, FILLER), stored[k:k + 0x4000])
                       for k in range(0, len(stored), 0x4000))
        crc = zlib.crc32(data)
        fixed = struct.pack("<HHHHHIII", 20, 0, method, 0, 0x5A21, crc, len(stored), len(data))
        body += b"PK\3\4" + fixed + struct.pack("<HH", len(raw), 0) + enc_name + enc
        central += b"PK\1\2" + struct.pack("<H", 20) + fixed + struct.pack("<HHHHHII", len(raw), 0, 0, 0, 0, 0x20, off)
        central += enc_name
    end = b"PK\5\6" + struct.pack("<HHHHIIH", 0, 0, len(entries), len(entries), len(central), len(body), 0)
    with open(path, "wb") as f:
        f.write(bytes(body) + bytes(central) + end)


def export(dumper, game, names, out):
    """The game's own bytes of the streamed sound files `names` (below sound/) -> {name: bytes}."""
    cmd = [dumper, "export-sounds", "--game", game, "--out", out]
    for n in names:
        cmd += ["--filter", "*" + n]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    found = {}
    for n in names:
        p = os.path.join(out, "streamed", "sound", *n.split("/"))
        if os.path.isfile(p):
            with open(p, "rb") as f:
                found[n] = f.read()
    return found


def locale(dumper, game, out):
    subprocess.run([dumper, "export-locale", "--game", game, "--out", out, "--filter", "SUBTITLE_SURVIVAL_SO_HQ_*"],
                   check=True, stdout=subprocess.DEVNULL)
    with open(out, encoding="utf-8-sig", newline="") as f:
        return {r["key"]: r["value"] for r in csv.DictReader(f)}


def pack(a):
    names = aliases.files(aliases.PACK)
    keys = aliases.subtitles(aliases.PACK)
    with tempfile.TemporaryDirectory() as tmp:
        data = export(a.dumper, a.survival, names, os.path.join(tmp, "s"))
        left = [n for n in names if n not in data]
        if left:
            data.update(export(a.dumper, a.lines, left, os.path.join(tmp, "l")))
        text = locale(a.dumper, a.lines, os.path.join(tmp, "loc.csv"))
    missing = [n for n in names if n not in data] + [k for k in keys if k not in text]
    if missing:
        raise SystemExit("not in the source installs: " + ", ".join(missing))
    main = os.path.join(a.out, "main")
    chinese = os.path.join(a.out, "client_shipRetail", "chinese")
    os.makedirs(main, exist_ok=True)
    os.makedirs(chinese, exist_ok=True)
    write_iwd(os.path.join(main, PACK_IWD), [("sound/" + n, data[n]) for n in names], iwd_key(a.c1))
    assets = [(36, 0xFFFFFFFF, codecs.encode_rawfile(PACK_ZONE, b"", {}))]
    assets += [(11, 0xFFFFFFFF, aliases.blob(n, v)) for n, v in aliases.PACK.items()]
    assets += [(27, 0xFFFFFFFF, codecs.encode_localize(k, text[k].encode("gbk"))) for k in keys]
    img = zoneimage.ZoneImage(blocks=[0] * 9, string_ptrs=[], strings=[], string_count_field=0, strings_ptr_field=0,
                              assets_ptr_field=0xFFFFFFFF, assets=assets)
    img.blocks[1] = 0x1000
    img.blocks[4] = sum(len(b) for _t, _p, b in assets) + 16 * len(assets) + MARGIN
    with open(os.path.join(chinese, PACK_ZONE + ".ff"), "wb") as f:
        f.write(container.write_unsigned(zoneimage.assemble(img), ""))
    print("%s: %d files, %d bytes; %s.ff: %d aliases, %d subtitles" % (
        PACK_IWD, len(names), os.path.getsize(os.path.join(main, PACK_IWD)), PACK_ZONE, len(aliases.PACK), len(keys)))
    print("Copy the contents of %s onto a 1.1.1.13 install." % os.path.abspath(a.out))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("zone")
    p = sub.add_parser("pack")
    p.add_argument("--survival", required=True, help="the CoD Online 2.6.9.7 folder")
    p.add_argument("--lines", required=True, help="the CoD Online 3.5.6.2 folder")
    p.add_argument("--c1", required=True, help="C1.exe of a 1.1.1.13 install")
    p.add_argument("--dumper", required=True, help="CodolSoundDumper.exe")
    p.add_argument("--out", default=os.path.join(ROOT, "build", "survival_audio"))
    a = ap.parse_args()
    zone() if a.cmd == "zone" else pack(a)


if __name__ == "__main__":
    main()
