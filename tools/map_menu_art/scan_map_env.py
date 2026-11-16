"""Survey the environment data a ported map zone carries (vision / sun rawfiles, sky and compass names).

python tools/map_menu_art/scan_map_env.py --exe <1.1.1.13 C1.exe> --out <dir> <map.ff> ...

For each zone: writes <out>/<map>/<rawfile path> for every vision/*.vision and sun/*.sun rawfile it holds, and prints
the sky / compass / loadscreen material and image names found in the zone image. Reads one zone at a time and pauses
between zones (the D: drive is shared with a running game).
"""
import argparse
import os
import re
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
from zonekit import container  # noqa: E402


def rawfiles(zone, prefixes):
    for m in re.finditer(rb"(?:%s)[A-Za-z0-9_./-]+\x00" % b"|".join(re.escape(p) for p in prefixes), zone):
        s = m.start()
        if s < 16 or zone[s - 4:s] != b"\xff\xff\xff\xff" or zone[s - 16:s - 12] != b"\xff\xff\xff\xff":
            continue
        clen, ulen = struct.unpack_from("<ii", zone, s - 12)
        name = zone[s:m.end() - 1].decode("latin-1")
        e = m.end()
        if clen > 0:
            data = zlib.decompressobj().decompress(zone[e:e + clen])
        else:
            data = zone[e:e + ulen]
        yield name, data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--pause", type=float, default=3.0)
    ap.add_argument("zones", nargs="+")
    a = ap.parse_args()
    for i, path in enumerate(a.zones):
        if i:
            time.sleep(a.pause)
        mapname = os.path.splitext(os.path.basename(path))[0]
        zone = container.read_any(path, a.exe)
        out = os.path.join(a.out, mapname)
        got = []
        for name, data in rawfiles(zone, [b"vision/", b"sun/", b"maps/createart/", b"maps/mp/"]):
            if name.endswith((".d3dbsp",)):
                continue
            p = os.path.join(out, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as f:
                f.write(data)
            got.append(name)
        names = sorted(set(m.group(0).decode() for m in re.finditer(
            rb"(?:sky|compass_map|loadscreen|\$levelbriefing)[A-Za-z0-9_]*", zone)))
        skies = [n for n in names if n.startswith("sky")]
        print("%s: rawfiles %s" % (mapname, " ".join(got)))
        print("%s: names %s" % (mapname, " ".join(n for n in names if not n.startswith("sky_") or n.endswith(("_ft",)) or "sky" in n[:4])[:2000]))
        sys.stdout.flush()


if __name__ == "__main__":
    main()
