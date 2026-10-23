# -*- coding: utf-8 -*-
"""String constants of a DoABC tag (SWF tag 82): read them and replace display literals in place.

A DoABC tag is flags u32, a name, then the ABC file: version, then the constant pool (ints, uints, doubles, strings,
namespaces, namespace sets, multinames), then everything else. Code refers to a string by its pool index, never by
offset, so a string can change length without touching the rest of the file.

replace() only changes strings that no namespace, multiname or metadata entry refers to, i.e. strings that are only
pushed by code (pushstring) or used as constant values: a pool entry that is also a property, class or package name
must keep its text, or the name changes with it.
"""
from __future__ import annotations

import struct

T_DOABC = 82


def _u30(b, at):
    v = 0
    for i in range(5):
        x = b[at + i]
        v |= (x & 0x7F) << (7 * i)
        if not x & 0x80:
            return v, at + i + 1
    raise ValueError("bad u30 at %d" % at)


def _w30(v):
    out = bytearray()
    while True:
        x = v & 0x7F
        v >>= 7
        if v:
            out.append(x | 0x80)
        else:
            out.append(x)
            return bytes(out)


def _split_tag(tag: bytes):
    cl = struct.unpack_from("<H", tag, 0)[0]
    code = cl >> 6
    if code != T_DOABC:
        raise ValueError("not a DoABC tag")
    hl = 6 if cl & 0x3F == 0x3F else 2
    body = tag[hl:]
    e = body.index(b"\0", 4)
    return body[:e + 1], body[e + 1:]


def _pool(abc: bytes):
    """-> (strings [(start, end, bytes)], string-count offset span, index set referenced by names, end of pool)"""
    at = 4
    n, at = _u30(abc, at)
    for _ in range(max(n - 1, 0)):
        _v, at = _u30(abc, at)
    n, at = _u30(abc, at)
    for _ in range(max(n - 1, 0)):
        _v, at = _u30(abc, at)
    n, at = _u30(abc, at)
    at += 8 * max(n - 1, 0)
    sc_start = at
    n, at = _u30(abc, at)
    strings = [None]
    for _ in range(max(n - 1, 0)):
        ln, s = _u30(abc, at)
        strings.append((at, s + ln, abc[s:s + ln]))
        at = s + ln
    named = set()
    n, at = _u30(abc, at)                       # namespaces
    for _ in range(max(n - 1, 0)):
        at += 1
        i, at = _u30(abc, at)
        named.add(i)
    n, at = _u30(abc, at)                       # namespace sets
    for _ in range(max(n - 1, 0)):
        c, at = _u30(abc, at)
        for _ in range(c):
            _v, at = _u30(abc, at)
    n, at = _u30(abc, at)                       # multinames
    for _ in range(max(n - 1, 0)):
        kind = abc[at]
        at += 1
        if kind in (0x07, 0x0D):                # QName(A): ns, name
            _v, at = _u30(abc, at)
            i, at = _u30(abc, at)
            named.add(i)
        elif kind in (0x0F, 0x10):              # RTQName(A): name
            i, at = _u30(abc, at)
            named.add(i)
        elif kind in (0x11, 0x12):              # RTQNameL(A)
            pass
        elif kind in (0x09, 0x0E):              # Multiname(A): name, ns set
            i, at = _u30(abc, at)
            named.add(i)
            _v, at = _u30(abc, at)
        elif kind in (0x1B, 0x1C):              # MultinameL(A): ns set
            _v, at = _u30(abc, at)
        elif kind == 0x1D:                      # TypeName: name index, params
            _v, at = _u30(abc, at)
            c, at = _u30(abc, at)
            for _ in range(c):
                _v, at = _u30(abc, at)
        else:
            raise ValueError("unknown multiname kind 0x%02X" % kind)
    return strings, named, at


def strings(tag: bytes):
    _h, abc = _split_tag(tag)
    return [s[2].decode("utf-8") for s in _pool(abc)[0][1:]]


def replace(tag: bytes, mapping: dict) -> bytes:
    """the tag with each pool string `old` (str) replaced by mapping[old]; every key must be found, as a literal only"""
    head, abc = _split_tag(tag)
    pool, named, _end = _pool(abc)
    found = {}
    for i, s in enumerate(pool):
        if s is None:
            continue
        text = s[2].decode("utf-8")
        if text in mapping:
            if i in named:
                raise ValueError("%r is also a name in this ABC, not replaced" % text)
            found[i] = mapping[text]
    missing = set(mapping) - {pool[i][2].decode("utf-8") for i in found}
    if missing:
        raise ValueError("strings not in the pool: %s" % ", ".join(sorted(missing)))
    out = bytearray()
    last = 0
    for i in sorted(found):
        start, end, _b = pool[i]
        new = found[i].encode("utf-8")
        out += abc[last:start] + _w30(len(new)) + new
        last = end
    out += abc[last:]
    body = head + bytes(out)
    return struct.pack("<HI", (T_DOABC << 6) | 0x3F, len(body)) + body
