# -*- coding: utf-8 -*-
"""Stock (and other input) zones read at build time: blobs by sha1, gfx movies by rawfile name."""
from __future__ import annotations

import hashlib
import os

from . import codecs, container, zoneimage


class ZoneSource:
    r"""Zones found by name in a list of folders (the 1.1.1.13 install's client_shipRetail\chinese)."""

    def __init__(self, exe: str, folders):
        self.exe = exe
        self.folders = [f for f in folders if f]
        self._zones = {}
        self._index = {}
        self._indexed = set()

    def path(self, name: str):
        fn = name if name.endswith(".ff") or ".ff." in name else name + ".ff"
        for d in self.folders:
            p = os.path.join(d, fn)
            if os.path.isfile(p):
                return p
        return None

    def zone(self, name: str):
        if name not in self._zones:
            p = self.path(name)
            if p is None:
                raise SystemExit("input zone %s not found in %s" % (name, "; ".join(self.folders)))
            self._zones[name] = zoneimage.parse(container.read_any(p, self.exe))
        return self._zones[name]

    def index(self, name: str):
        """Add every blob of zone `name` to the sha1 index."""
        if name in self._indexed:
            return
        for _t, _p, b in self.zone(name).assets:
            self._index.setdefault(hashlib.sha1(b).hexdigest(), b)
        self._indexed.add(name)
        self._zones.pop(name, None)       # the index keeps the blobs it needs; drop the parsed image

    def blob(self, sha1: str):
        return self._index.get(sha1)

    def rawfile(self, ref: str) -> bytes:
        """'<zone>:<rawfile name>' -> the rawfile's data."""
        zn, _, rn = ref.partition(":")
        for t, _p, b in self.zone(zn).assets:
            if t == 36:
                try:
                    name, data, _ = codecs.decode_rawfile(b)
                except codecs.NotDecodable:
                    continue
                if name == rn:
                    return data
        raise SystemExit("%s: no rawfile %s" % (zn, rn))
