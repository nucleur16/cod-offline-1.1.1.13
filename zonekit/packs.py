# -*- coding: utf-8 -*-
r"""Consolidated patch zones: several source zones (parts) built into one installed zone.

Every source zone keeps its own sources (zone_source/<part>.csv + zones/<part>/), its own shipping filter (zonelist
`maps=`) and its own --verify against the file it came from. What installs is a pack: zone_packs/<pack>.csv names its
parts in order, and the build concatenates each part's assets (after that part's shipping filter) into one zone.

    // comment
    >zone,<pack>                  the installed name; it decides the load list (kernel33.c PatchZoneStage):
                                  *_postgfx_mp after code_post_gfx, *_ui_mp ahead of the UI zones, else ahead of
                                  code_pre_gfx; english* / chinese* load in one language only
    >install,client_shipRetail\patch
    >filetime,hi,lo  >rsa,hex     the container's recorded key (drawn and written back on the first build)
    >zlib,9
    >build-md5,<md5>              the reviewed shipping build (--maps ship); >build-md5-all: the --maps all build
    part,<zone>[,only=t1+t2][,exclude=t1+t2]
                                  a source zone's assets, in zone order; only= / exclude= take asset types

Rules the build enforces:
  * every part is a flat zone (no script strings, no back references: a C#-written zone such as
    infected_cyborg_postgfx_mp points into its own runtime blocks, so its assets cannot move and it ships on
    its own), except that one part of a pack may carry script strings when every asset of it is proven
    self-contained (STRINGS_PART_TYPES: an xanim the parser reads as all inline data, xanim_opposite_arm.parse):
    its assets name script strings only by index into its own table, so the pack takes that table whole, in
    order, and the indices keep their meaning (m1911_arms_diag in common_extras);
  * a part loads in the same list as the pack (the old name's suffix gives the same stage), and in the same languages,
    except where a pack takes only some asset types of a part (usermaps_patch's GBK strings go to the Chinese pack);
  * C1 keeps the first asset linked under a name (kernel33.c, "first entry it links"); the old zones loaded in
    alphabetical order, so the parts are listed in that order and an asset whose (type, name) an earlier part
    already carries is the one the old install never used: it is left out and reported ("shadowed").
  * a part belongs to one pack, or is split over packs by asset type; every source zone that is not a part ships on
    its own.

Kernel33 skips an old zone name when the pack that replaced it is in the patch folder (patch_locale_policy.h
PatchZoneMergedInto); tests/test_zone_packs.py checks that table against these manifests.
"""
from __future__ import annotations

import os

from . import build, container, manifest, zoneimage

PACK_DIR = "zone_packs"
SLACK = 0x1000          # per merged runtime block other than block 4: allocation alignment between parts
# asset types a part with script strings may carry, each with the check that its blob is all inline data (no back
# reference into a runtime block, which would move with the pack's longer asset table) and that every script
# string index it uses is a string of the part's own table
STRINGS_PART_TYPES = ("xanim",)


class PackError(Exception):
    pass


def stage(name):
    """kernel33.c PatchZoneStage: the load list a patch zone of this name joins."""
    n = name.lower()
    if len(n) > 11 and n.endswith("_postgfx_mp"):
        return "post_gfx"
    if len(n) > 6 and n.endswith("_ui_mp"):
        return "ui"
    return "pre_gfx"


def locales(name):
    """patch_locale_policy.h PatchZoneLocaleAllowed: the languages (en, zh, native) a zone of this name loads in."""
    n = name.lower()
    if n.startswith("english"):
        return frozenset(["en"])
    if n.startswith("chinese"):
        return frozenset(["zh", "native"])
    return frozenset(["en", "zh", "native"])


def names(repo):
    d = os.path.join(repo, PACK_DIR)
    if not os.path.isdir(d):
        return []
    return sorted(f[:-4] for f in os.listdir(d) if f.endswith(".csv"))


def load(repo, pack):
    path = os.path.join(repo, PACK_DIR, pack + ".csv")
    with open(path, encoding="utf-8") as f:
        m = manifest.read(f.read())
    if m.get("zone") != pack:
        raise PackError("%s: >zone is %r" % (path, m.get("zone")))
    return m


def parts(m):
    """-> [(part zone, filter)] where filter(type name) says whether an asset of that type comes along."""
    out = []
    for t, name, q in m.assets:
        if t != "part":
            raise PackError("%s: line %r: only 'part' lines are allowed" % (m.get("zone"), t))
        only = set(q["only"].split("+")) if q.get("only") else None
        exclude = set(q["exclude"].split("+")) if q.get("exclude") else set()
        out.append((name, (lambda o, e: lambda t: (o is None or t in o) and t not in e)(only, exclude), q))
    return out


def part_index(repo):
    """-> {part zone: [packs]} over every pack. A zone may be split over several packs only by asset type (every
    occurrence with only= or exclude=); the checker (tests/check_zone_packs.py) proves each asset lands once."""
    idx, partial = {}, {}
    for p in names(repo):
        for part, _f, q in parts(load(repo, p)):
            idx.setdefault(part, []).append(p)
            partial.setdefault(part, []).append("only" in q or "exclude" in q)
    for part, ps in idx.items():
        if len(ps) > 1 and not all(partial[part]):
            raise PackError("%s is a whole part of one of %s: split a zone over packs only with only= / exclude="
                            % (part, ", ".join(ps)))
    return idx


def part_assets(repo, part, stocks, maps, ffdec=None, compile_as=False, log=print):
    """One source zone as the old build made it: -> (manifest, zone image, [(type, name, (tid, ptr, blob))])."""
    m = build.load(repo, part)
    img, plan = build.assemble_filtered(repo, part, m, stocks, ffdec=ffdec, compile_as=compile_as, log=log, maps=maps)
    z = zoneimage.parse(img)
    kept = [(t, n) for t, n, _q in m.assets if (t, n) not in plan.drop]
    if len(kept) != len(z.assets):
        raise PackError("%s: %d manifest assets after the filter, %d in the zone" % (part, len(kept), len(z.assets)))
    return m, z, plan, [(t, n, a) for (t, n), a in zip(kept, z.assets)]


def strings_part_check(part, items, strings):
    """Raise PackError unless every asset of a part with script strings can move into a pack unchanged."""
    from . import xanim_opposite_arm
    for t, name, (_tid, _ptr, blob) in items:
        if t not in STRINGS_PART_TYPES:
            raise PackError("%s has script strings and a %s (%s): only %s assets are proven to move with their "
                            "string table; it must ship on its own" % (part, t, name, ", ".join(STRINGS_PART_TYPES)))
        try:
            xanim_opposite_arm.parse(blob, strings)
        except (xanim_opposite_arm.Unsupported, ValueError, IndexError) as e:
            raise PackError("%s: xanim %s is not all inline data (%s); it must ship on its own" % (part, name, e))


def assemble(repo, pack, stocks, maps, ffdec=None, compile_as=False, log=print):
    """-> (zone image bytes, report dict). report: parts, per-part counts, shadowed duplicates, dropped maps."""
    m = load(repo, pack)
    pstage, ploc = stage(pack), locales(pack)
    assets = [(zoneimage.type_id("rawfile"), 0xFFFFFFFF, _marker(pack))]
    seen = {("rawfile", pack.lower()): pack}
    blocks = [0] * 9
    used = [0] * 9
    xal = None
    report = dict(parts=[], shadowed=[], dropped_maps=set(), notes=[])
    strings_part = None     # (part, its zone image): the one part whose script string table the pack takes
    for part, keep, q in parts(m):
        pm, z, plan, items = part_assets(repo, part, stocks, maps, ffdec, compile_as, log)
        if not z.flat or z.strings:
            if strings_part is not None:
                raise PackError("%s: parts %s and %s both have script strings; a pack takes one part's table"
                                % (pack, strings_part[0], part))
            if "only" in q or "exclude" in q:
                raise PackError("%s: part %s has script strings; it cannot be split by type" % (pack, part))
            strings_part_check(part, items, z.strings)
            strings_part = (part, z)
        if stage(part) != pstage:
            raise PackError("%s (%s) cannot take %s, which loads in the %s list" % (pack, pstage, part, stage(part)))
        partial = "only" in q or "exclude" in q
        if not partial and locales(part) != ploc:
            raise PackError("%s loads in %s, its part %s in %s" % (pack, sorted(ploc), part, sorted(locales(part))))
        # the strings pointer field belongs to the table (taken from the strings part below), the assets one to all
        fields = (z.strings_ptr_field if z.flat else None, z.assets_ptr_field)
        if xal is None:
            xal = fields
        elif fields[1] != xal[1] or None not in (fields[0], xal[0]) and fields[0] != xal[0]:
            raise PackError("%s: part %s has the XAssetList fields %s, the others %s" % (pack, part, fields, xal))
        elif xal[0] is None:
            xal = fields
        n = 0
        for t, name, a in items:
            if not keep(t):
                continue
            key = (t, name.lower())
            if key in seen:
                report["shadowed"].append((t, name, part, seen[key]))
                continue
            seen[key] = part
            assets.append(a)
            n += 1
        for i, v in enumerate(z.blocks):
            if i != 4 and v:
                blocks[i] += v
                used[i] += 1
        blocks[4] += z.blocks[4]
        report["parts"].append((part, n, len(items)))
        report["dropped_maps"] |= plan.dropped_maps
        report["notes"] += ["%s: %s" % (part, s) for s in plan.notes]
    for i in range(9):
        if i != 4 and used[i] > 1:
            blocks[i] += SLACK
    # the strings part's table, whole and in order (its assets name strings by index), else none
    sz = strings_part[1] if strings_part else None
    table = dict(string_ptrs=list(sz.string_ptrs), strings=list(sz.strings), string_count_field=sz.string_count_field,
                 strings_ptr_field=sz.strings_ptr_field) if sz else         dict(string_ptrs=[], strings=[], string_count_field=0, strings_ptr_field=xal[0] if xal[0] is not None else 0)
    report["strings"] = (strings_part[0], sz.string_count_field) if sz else None
    # block 4 (the zone's own data): the sum of the parts' (each with its margin) covers every byte they allocated
    auto = sum(len(b) for _t, _p, b in assets) + 16 * len(assets) + 0x10000
    auto += 4 * len(table["string_ptrs"]) + sum(len(s) + 1 for s in table["strings"] if s is not None)
    blocks[4] = max(blocks[4], auto)
    img = zoneimage.ZoneImage(blocks=blocks, assets_ptr_field=xal[1], assets=assets, **table)
    return zoneimage.assemble(img), report


def _marker(pack):
    """The empty rawfile named after the zone that every flat zone here starts with."""
    from . import codecs
    return codecs.encode_rawfile(pack, b"", {})


def build_pack(repo, pack, exe, stocks, out_root, maps, fresh_key=False, ffdec=None, compile_as=False, log=print):
    """-> (output path, zone image, manifest, report). A pack without a recorded key gets one, written back."""
    m = load(repo, pack)
    zone, report = assemble(repo, pack, stocks, maps, ffdec, compile_as, log)
    ft = m.settings.get("filetime")
    rsa = m.get("rsa")
    if fresh_key or not ft or not rsa:
        data = container.write_unsigned(zone, exe, level=int(m.get("zlib", "9")))
        if not fresh_key:
            hi, lo = (int.from_bytes(data[0x0D + 4 * i:0x11 + 4 * i], "little") for i in range(2))
            set_settings(repo, pack, {"filetime": ["%#010x" % hi, "%#010x" % lo],
                                      "rsa": [data[container.HEADER_LEN:container.BODY_OFF].hex()]})
            log("  %s: no recorded key, drew one and recorded it in %s/%s.csv" % (pack, PACK_DIR, pack))
    else:
        data = container.write_unsigned(zone, exe, filetime=tuple(int(v, 0) for v in ft), rsa_blob=bytes.fromhex(rsa),
                                        level=int(m.get("zlib", "9")))
    out_dir = os.path.join(out_root, m.get("install").replace("\\", os.sep))
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, pack + ".ff")
    with open(out, "wb") as f:
        f.write(data)
    return out, zone, m, report


def md5_key(maps_mode):
    return "build-md5" if maps_mode == "ship" else "build-md5-all"


def set_settings(repo, pack, values):
    """Rewrite >key lines of zone_packs/<pack>.csv in place (added after >install when missing)."""
    path = os.path.join(repo, PACK_DIR, pack + ".csv")
    with open(path, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    for key, vals in values.items():
        line = ">" + ",".join([key] + list(vals))
        at = [i for i, l in enumerate(lines) if l.split(",", 1)[0] == ">" + key]
        if at:
            lines[at[0]] = line
        else:
            anchor = [i for i, l in enumerate(lines) if l.startswith(">")]
            lines.insert(anchor[-1] + 1 if anchor else 0, line)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(lines))
