# -*- coding: utf-8 -*-
r"""shipping_maps.txt: which usermaps ship to players, and the filter that keeps the others' UI out of a shipping build.

The patch zones carry front-end data for every usermap the project has worked on: the create-room map pool row
(mp/comaplist.csv), the vlobby arena entry (mp/basemaps.arena), display-name localize strings, the map-select image
(sfproto_<map>_usermap), vision overrides, and the per-map sound channel fixes (files\...\sound_channels.txt). A
map that is still being worked on must not reach players through them: a pool row for a map the player does not
have is a dead entry (or a failed load), and a map-select image whose pixels are in an iwd the player does not have
is a COM_FATAL at client startup.

shipping_maps.txt (repository root) lists every usermap with its state:

    <map> <state>       # comment
    state: ship   in the shipping build (python scripts/build_zones.py; output\)
           dev    only in the all-maps build (python scripts/build_zones.py --maps all; output_dev\)

Every usermap a filtered asset names must be listed: an unlisted map fails the build, so a new map's UI can only
ship after someone writes "ship" next to it.

Which data is per map is declared per zone in zonelist.py (`maps=`):
    table=(<stringtable>, <map column>, <stock zone>)
        the stock zone's table is the prefix (checked cell by cell through C1's cell hashes); every row after it is a
        usermap row, owned by the map in <map column>.
    stock_edits={<map>: [<column>, ...]}
        cells of the stock prefix this zone changes on purpose (the stock row of <map>, those columns); every other
        stock cell must still be the stock zone's. The zone that owns a stringtable makes every change to it: a second
        zone with the same table would replace this one's (or be replaced) at load.
    arena=(<rawfile>, <stock zone>)
        the stock zone's rawfile is the prefix (checked byte for byte); every { } entry after it is a usermap entry,
        owned by the map its `map "..."` key names.
    assets=["<type>:<name pattern with {map}>", ...]
        assets owned by one map (e.g. "image:sfproto_{map}_usermap").
    localize=True
        a localize entry of the zone that only dropped rows/entries name is dropped with them.
    localize_from=<zone>
        the zone's localize entries are another zone's in another language (codol_english_usermaps: usermaps_patch's
        map names in English); each is dropped when that zone's filter drops it, and each must exist there.
FILES maps a files\ path to its per-map line filter (first word of a line = the map zone).
"""
from __future__ import annotations

import os
import re
import struct

from . import codecs

MANIFEST = "shipping_maps.txt"
STATES = ("ship", "dev")
MODES = ("ship", "all")

# files\ copied into the output whose lines are per map: "<map zone> ..." lines and "# <map zone> (...)" section headers
FILES = {"client_shipRetail/patch/sound_channels.txt"}


class ShippingError(Exception):
    pass


class ShippingMaps:
    """The parsed manifest and the build mode ("ship": only ship maps; "all": every listed map)."""

    def __init__(self, states, mode="ship", path=MANIFEST):
        if mode not in MODES:
            raise ShippingError("maps mode %r: use one of %s" % (mode, ", ".join(MODES)))
        self.states = states
        self.mode = mode
        self.path = path

    @classmethod
    def load(cls, repo, mode="ship"):
        path = os.path.join(repo, MANIFEST)
        if not os.path.isfile(path):
            raise ShippingError("%s is missing: it lists the maps whose UI ships (see zonekit/shipping.py)" % MANIFEST)
        states = {}
        with open(path, encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                words = line.split("#", 1)[0].split()
                if not words:
                    continue
                if len(words) != 2 or words[1] not in STATES:
                    raise ShippingError("%s:%d: expected '<map> <%s>', got %r" % (MANIFEST, n, "|".join(STATES),
                                                                                    line.strip()))
                m = words[0].lower()
                if m in states:
                    raise ShippingError("%s:%d: %s is listed twice" % (MANIFEST, n, m))
                states[m] = words[1]
        return cls(states, mode, path)

    def by_state(self, state):
        return [m for m, s in self.states.items() if s == state]

    def ships(self, mapname, where):
        """True when `mapname`'s data goes into this build; an unlisted map is an error."""
        state = self.states.get(mapname.lower())
        if state is None:
            raise ShippingError("%s names the usermap %s, which %s does not list: add it as '%s ship' or '%s dev'"
                                % (where, mapname, MANIFEST, mapname, mapname))
        return self.mode == "all" or state == "ship"


class Plan:
    """What the filter changes in one zone: assets left out, replaced stringtable rows / rawfile data, and a report."""

    def __init__(self):
        self.drop = set()          # (type, name)
        self.rows = {}             # stringtable name -> rows
        self.rawfiles = {}         # rawfile name -> data
        self.dropped_maps = set()
        self.notes = []

    def __bool__(self):
        return bool(self.drop or self.dropped_maps)


def _pattern(p):
    """'image:sfproto_{map}_usermap' -> (type, compiled regex with a `map` group)."""
    t, _, name = p.partition(":")
    head, _, tail = name.partition("{map}")
    return t, re.compile(re.escape(head) + r"(?P<map>[A-Za-z0-9_]+?)" + re.escape(tail) + r"\Z")


def _stock_table_hashes(stocks, zone, name):
    """The stock stringtable's (columns, rows, cell hashes); its cells may be back-referenced, the hashes never are."""
    for t, _p, blob in stocks.zone(zone).assets:
        if t == 37 and codecs.guess_name(t, blob) == name:
            _np, cols, rows, _cp = struct.unpack_from("<4I", blob, 0)
            at = blob.index(b"\0", 16) + 1
            return cols, rows, [struct.unpack_from("<2I", blob, at + 8 * i)[1] for i in range(cols * rows)]
    raise ShippingError("stock zone %s has no stringtable %s" % (zone, name))


def _arena_entries(text):
    """Arena text -> [(gap before the entry, entry)], trailing text. Entries are { } blocks (no nesting)."""
    out, at = [], 0
    for m in re.finditer(rb"\{[^{}]*\}", text):
        out.append((text[at:m.start()], m.group(0)))
        at = m.end()
    return out, text[at:]


def _arena_map(entry):
    m = re.search(rb'\bmap\s+"([^"]+)"', entry)
    return m.group(1).decode("latin-1") if m else None


def plan(repo, zone_name, m, cfg, stocks, maps):
    """The shipping filter for one zone (zonelist entry `cfg`), or an empty Plan when it has no per-map data."""
    p = Plan()
    spec = (cfg or {}).get("maps")
    if not spec or maps is None:
        return p
    zdir = os.path.join(repo, "zones", zone_name)
    params = {(t, n): q for t, n, q in m.assets}
    kept_refs, dropped_refs = set(), set()

    def owner(mapname, where):
        if mapname.startswith("coop_survival_"):
            # Survival rows describe MP levels, never a session .ff archive.
            keep = mapname[len("coop_survival_"):] in spec.get("survival_levels", ())
        else:
            keep = maps.ships(mapname, "%s: %s" % (zone_name, where))
        if not keep:
            p.dropped_maps.add(mapname.lower())
        return keep

    if "table" in spec:
        name, col, stock_zone = spec["table"]
        q = params.get(("stringtable", name))
        if q is None:
            raise ShippingError("%s: no stringtable %s in its manifest" % (zone_name, name))
        with open(os.path.join(zdir, "stringtable", name), encoding="utf-8", newline="") as g:
            rows = codecs.read_csv(g.read(), q.get("enc", "ascii"))
        cols, nstock, hashes = _stock_table_hashes(stocks, stock_zone, name)
        edits = {k.lower(): set(v) for k, v in spec.get("stock_edits", {}).items()}

        def stock_row(r):
            mine = [codecs.st_hash(c) for c in rows[r]]
            theirs = hashes[r * cols:(r + 1) * cols]
            free = edits.get(rows[r][col].decode("latin-1").lower(), ()) if len(rows[r]) > col else ()
            return len(mine) == len(theirs) and all(a == b or i in free for i, (a, b) in enumerate(zip(mine, theirs)))
        if cols != int(q["columns"]) or len(rows) < nstock or not all(stock_row(r) for r in range(nstock)):
            raise ShippingError("%s: the first %d rows of %s are not %s's stock table any more; the filter takes the "
                                "rows after them as the usermap rows" % (zone_name, nstock, name, stock_zone))
        out = rows[:nstock]
        for row in rows[:nstock]:
            kept_refs.update(row)
        for row in rows[nstock:]:
            mp = row[col].decode("latin-1")
            if owner(mp, "%s row %s" % (name, row[0].decode("latin-1"))):
                out.append(row)
                kept_refs.update(row)
            else:
                dropped_refs.update(row)
        if len(out) != len(rows):
            p.rows[name] = out
            p.notes.append("%s: %d of %d usermap rows" % (name, len(out) - nstock, len(rows) - nstock))

    if "arena" in spec:
        name, stock_zone = spec["arena"]
        with open(os.path.join(zdir, "rawfile", name), "rb") as g:
            data = g.read()
        stock = stocks.rawfile("%s:%s" % (stock_zone, name))
        if not data.startswith(stock):
            raise ShippingError("%s: %s does not start with %s's stock text any more; the filter takes the entries "
                                "after it as the usermap entries" % (zone_name, name, stock_zone))
        kept_refs.update(re.findall(rb'"([^"]*)"', stock))
        entries, trailing = _arena_entries(data[len(stock):])
        out, n_kept = bytearray(stock), 0
        for gap, entry in entries:
            mp = _arena_map(entry)
            if mp is None:
                raise ShippingError("%s: an entry of %s after the stock text has no map key" % (zone_name, name))
            refs = set(re.findall(rb'"([^"]*)"', entry))
            if owner(mp, "%s entry" % name):
                out += gap + entry
                kept_refs.update(refs)
                n_kept += 1
            else:
                dropped_refs.update(refs)
        out += trailing
        if n_kept != len(entries):
            p.rawfiles[name] = bytes(out)
            p.notes.append("%s: %d of %d usermap entries" % (name, n_kept, len(entries)))

    for pat in spec.get("assets", []):
        t, rx = _pattern(pat)
        n_all = n_drop = 0
        for at, an, _q in m.assets:
            hit = at == t and rx.match(an)
            if not hit:
                continue
            n_all += 1
            if not owner(hit.group("map"), "%s %s" % (at, an)):
                p.drop.add((at, an))
                n_drop += 1
        if n_drop:
            p.notes.append("%s: %d of %d" % (pat.replace("{map}", "<map>"), n_all - n_drop, n_all))

    if spec.get("localize"):
        names = [an for at, an, _q in m.assets if at == "localize"]
        gone = [an for an in names if an.encode("latin-1") in dropped_refs and an.encode("latin-1") not in kept_refs]
        p.drop.update(("localize", an) for an in gone)
        if gone:
            p.notes.append("localize: %d of %d (%s dropped)" % (len(names) - len(gone), len(names), " ".join(gone)))
    if spec.get("localize_from"):
        from . import build, zonelist    # build imports this module
        other = spec["localize_from"]
        om = build.load(repo, other)
        op = plan(repo, other, om, zonelist.ZONES.get(other), stocks, maps)
        theirs = {an for at, an, _q in om.assets if at == "localize"}
        names = [an for at, an, _q in m.assets if at == "localize"]
        missing = [an for an in names if an not in theirs]
        if missing:
            raise ShippingError("%s: localize entries %s are not in %s, whose strings these translate"
                                % (zone_name, " ".join(missing), other))
        gone = [an for an in names if ("localize", an) in op.drop]
        p.drop.update(("localize", an) for an in gone)
        p.dropped_maps |= op.dropped_maps
        if gone:
            p.notes.append("localize: %d of %d (as %s)" % (len(names) - len(gone), len(names), other))
    return p


def filter_file(rel, data, maps):
    """A files\\ file as it ships: (data, dropped maps). Files not in FILES pass unchanged."""
    if maps is None or rel.replace(os.sep, "/") not in FILES:
        return data, set()
    out, dropped = [], set()
    head = re.compile(rb"#\s*([A-Za-z0-9_]+)\s+\(")
    for line in data.splitlines(keepends=True):
        s = line.strip()
        if not s:
            out.append(line)
            continue
        if s.startswith(b"#"):
            h = head.match(s)
            mp = h.group(1).decode("latin-1") if h and h.group(1).lower().startswith((b"mp_", b"zm_")) else None
        else:
            mp = s.split()[0].decode("latin-1")
        if mp is not None and not maps.ships(mp, "%s (%s)" % (rel, mp)):
            dropped.add(mp.lower())
            continue
        out.append(line)
    return b"".join(out), dropped
