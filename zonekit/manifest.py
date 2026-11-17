# -*- coding: utf-8 -*-
r"""zone_source/<zone>.csv: the build manifest of one zone, in IW zone_source style.

    // comment
    >key,value[,value...]          zone settings (container, key, timestamp, blocks, inputs)
    type,name[,param=value...]     one line per asset, in zone order

Asset lines (files are under zones/<zone>/):
    rawfile,<name>[,zlib=N][,tail=hex|-][,gfx=<stock zone>:<rawfile>]
                                   rawfile/<name>  (gfx=: rawfile/<name>.gfxpatch/, a patch over that stock movie)
    localize,<NAME>                the next entry named NAME in localize.str
    stringtable,<name>,columns=N[,enc=E][,emptyptr=null][,cellsptr=..]
                                   stringtable/<name> (CSV, UTF-8; cells are E in the zone)
    menufile,<name>,src=<path>.menu   a menu file (MenuList), compiled from its .menu source (zonekit/menu_source.py)
    menu,<name>,src=<path>.menu    a menu (menuDef_t) on its own, compiled from a .menu source with one menuDef
    <type>,<name>,file=<path>      any other asset, as the raw blob <path> (relative to zones/<zone>/)
    <type>,<name>,file=@stock:<sha1>   a blob the build takes unchanged from a stock zone named by >stock
    ptr=0x..                       the directory entry's pointer field when it is not 0xFFFFFFFF
"""
from __future__ import annotations

import csv
import io


class Manifest:
    def __init__(self):
        self.settings = {}      # key -> [values]
        self.assets = []        # (type, name, {params})
        self.comments = []

    def get(self, key, default=None):
        v = self.settings.get(key)
        return v[0] if v else default


def write(m: Manifest) -> str:
    buf = io.StringIO(newline="")
    for c in m.comments:
        buf.write("// %s\n" % c)
    w = csv.writer(buf, lineterminator="\n")
    for k, vals in m.settings.items():
        w.writerow([">" + k] + list(vals))
    for t, name, params in m.assets:
        w.writerow([t, name] + ["%s=%s" % kv for kv in params.items()])
    return buf.getvalue()


def read(text: str) -> Manifest:
    m = Manifest()
    lines = [l for l in text.split("\n")]
    body = []
    for l in lines:
        if l.startswith("//"):
            m.comments.append(l[2:].strip())
        elif l.strip():
            body.append(l)
    for row in csv.reader(io.StringIO("\n".join(body), newline="")):
        if not row:
            continue
        if row[0].startswith(">"):
            m.settings[row[0][1:]] = row[1:]
            continue
        params = {}
        for p in row[2:]:
            k, _, v = p.partition("=")
            params[k] = v
        m.assets.append((row[0], row[1] if len(row) > 1 else "", params))
    return m
