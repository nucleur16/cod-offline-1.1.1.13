# -*- coding: utf-8 -*-
r"""Fast file containers of CoD Online 1.1.1.13 (C1.exe).

Two kinds exist in an install:

  IWffu100 (unsigned, what every patch / usermap zone here is):
      +0x00 "IWffu100"  +0x08 u32 version 0x14E  +0x0C u8 1  +0x0D u32 filetime hi  +0x11 u32 filetime lo
      +0x15 RSA-OAEP(SHA-256) of AES-192 key (24) + IV (16), 256 bytes
      +0x115 zlib(zone image); C1 decrypts (AES-CTR) only the body bytes inside its first 0x40000-byte read chunk
             (stream init 0x004B5AC0 -> 0x004B5B7D) and inflates the rest as stored.
  IWff0100 (signed, the stock zones): 0x2000-byte blocks, each AES-CTR with its own IV, a signed header block and
      IV-table blocks every 257 blocks (alpha0022 zones/zone_assets.py, MEASURED there).

The RSA key is the DER private key inside C1.exe at file offset 0x83C248 (1191 bytes), kept in this folder as
c1_zone_key.der so a build needs no C1.exe. The `exe` arguments are only read when that file is missing.
"""
from __future__ import annotations

import hashlib
import os
import struct
import zlib

MAGIC_UNSIGNED = b"IWffu100"
MAGIC_SIGNED = b"IWff0100"
ZONE_VERSION = 0x14E
HEADER_LEN = 0x15
RSA_LEN = 0x100
BODY_OFF = HEADER_LEN + RSA_LEN
READ_CHUNK = 0x40000
ENCRYPTED = READ_CHUNK - BODY_OFF
KEY_OFF, KEY_LEN = 0x83C248, 1191
KEY_SHA = "3a2f7891ac98f1fdf6a9a221c5586725157df270d6831e0ff82c383cd262cbb5"

_keys: dict = {}


KEY_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "c1_zone_key.der")


def rsa_key(exe: str | None = None):
    from Crypto.PublicKey import RSA
    if "" not in _keys:
        if os.path.isfile(KEY_FILE):
            src, der = KEY_FILE, open(KEY_FILE, "rb").read()
        elif exe and os.path.isfile(exe):
            src = exe
            with open(exe, "rb") as f:
                f.seek(KEY_OFF)
                der = f.read(KEY_LEN)
        else:
            raise SystemExit("no zone key: %s is missing and no C1.exe of 1.1.1.13 was given" % KEY_FILE)
        if hashlib.sha256(der).hexdigest() != KEY_SHA:
            raise SystemExit("%s: not the 1.1.1.13 zone key" % src)
        _keys[""] = RSA.import_key(der)
    return _keys[""]


def ctr(key: bytes, iv: bytes, data: bytes) -> bytes:
    """AES-CTR as LibTomCrypt runs it in C1: E(IV) first, counter incremented little-endian."""
    from Crypto.Cipher import AES
    from Crypto.Util import Counter
    c = Counter.new(128, initial_value=int.from_bytes(iv, "little"), little_endian=True)
    return AES.new(key, AES.MODE_CTR, counter=c).encrypt(data)


def _oaep(exe):
    from Crypto.Cipher import PKCS1_OAEP
    from Crypto.Hash import SHA256
    return PKCS1_OAEP.new(rsa_key(exe), hashAlgo=SHA256)


class Unsigned:
    """A parsed IWffu100 file: header fields, key material and the inflated zone image."""

    def __init__(self, filetime, rsa_blob, key, iv, zone, compressed):
        self.filetime = filetime          # (hi, lo)
        self.rsa_blob = rsa_blob          # the 256 wrapped bytes, kept so a rebuild can reuse the same key
        self.key, self.iv = key, iv
        self.zone = zone
        self.compressed = compressed      # the zlib stream as stored (decrypted)


def read_unsigned(data: bytes, exe: str) -> Unsigned:
    if data[:8] != MAGIC_UNSIGNED:
        raise ValueError("not an IWffu100 file (%r)" % data[:8])
    version, flag, hi, lo = struct.unpack_from("<IBII", data, 8)
    if version != ZONE_VERSION:
        raise ValueError("zone version %#x, expected %#x" % (version, ZONE_VERSION))
    blob = data[HEADER_LEN:BODY_OFF]
    mat = _oaep(exe).decrypt(blob)
    key, iv = mat[:24], mat[24:40]
    body = data[BODY_OFF:]
    stream = ctr(key, iv, body[:ENCRYPTED]) + body[ENCRYPTED:]
    d = zlib.decompressobj()
    zone = d.decompress(stream) + d.flush()
    if d.unused_data:
        raise ValueError("%d bytes after the zlib stream" % len(d.unused_data))
    return Unsigned((hi, lo), blob, key, iv, zone, stream)


def compress(zone: bytes, level: int = 9) -> bytes:
    return zlib.compress(zone, level)


def write_unsigned(zone: bytes, exe: str, *, filetime=None, rsa_blob: bytes | None = None, level: int = 9,
                   compressed: bytes | None = None) -> bytes:
    """IWffu100 file of `zone`. With `rsa_blob` (a previous build's wrapped key) the key and IV are the same as that
    build's, so an unchanged zone comes out byte for byte; without it a fresh key is drawn. `compressed` replaces the
    zlib stream (it must inflate to `zone`)."""
    if rsa_blob is not None:
        mat = _oaep(exe).decrypt(rsa_blob)
        key, iv = mat[:24], mat[24:40]
    else:
        key, iv = os.urandom(24), os.urandom(16)
        rsa_blob = _oaep(exe).encrypt(key + iv)
    if filetime is None:
        import datetime
        now = datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None)
        delta = now - datetime.datetime(1601, 1, 1)
        v = (delta.days * 86400 + delta.seconds) * 10000000 + delta.microseconds * 10
        filetime = ((v >> 32) & 0xFFFFFFFF, v & 0xFFFFFFFF)
    body = compressed if compressed is not None else compress(zone, level)
    head = MAGIC_UNSIGNED + struct.pack("<IBII", ZONE_VERSION, 1, filetime[0], filetime[1])
    return head + rsa_blob + ctr(key, iv, body[:ENCRYPTED]) + body[ENCRYPTED:]


def read_signed(data: bytes, exe: str, verify: bool = True) -> bytes:
    """Inflated zone image of a stock IWff0100 file (alpha0022 zones/zone_assets.py decrypt_ff, MEASURED there)."""
    if data[:8] != MAGIC_SIGNED:
        raise ValueError("not an IWff0100 file (%r)" % data[:8])
    body = data[BODY_OFF:]
    if len(body) % 0x2000:
        raise ValueError("payload is not 0x2000-block aligned")
    mat = _oaep(exe).decrypt(data[HEADER_LEN:BODY_OFF])
    key, iv0 = mat[:24], mat[24:40]
    n = len(body) // 0x2000
    blk = lambda i: body[i * 0x2000:(i + 1) * 0x2000]  # noqa: E731
    hdr = ctr(key, iv0, blk(0))
    if hdr[:8] != b"IWffs100":
        raise ValueError("bad inner header magic %r" % hdr[:8])
    if verify and hashlib.sha256(hdr[0x12C:0x1FD0]).digest() != hdr[0x0C:0x2C]:
        raise ValueError("header section-table sha256 mismatch")
    out = bytearray()
    i, sect, ivtab, k = 1, 0, b"", 0
    while i < n:
        if k == 0:
            ent = hdr[0x150 + 32 * sect:0x150 + 32 * sect + 32]
            pt = ctr(key, ent[:16], blk(i))
            if verify and hashlib.sha256(pt).digest() != ent:
                raise ValueError("IV-table block %d hash mismatch" % i)
            ivtab, k = pt, 1
        else:
            ent = ivtab[32 * (k - 1):32 * k]
            pt = ctr(key, ent[:16], blk(i))
            if verify and hashlib.sha256(pt).digest() != ent:
                raise ValueError("data block %d hash mismatch" % i)
            out += pt
            k += 1
            if k == 0x101:
                sect, k = sect + 1, 0
        i += 1
    return zlib.decompressobj().decompress(bytes(out))


def read_any(path: str, exe: str) -> bytes:
    """Zone image of any 1.1.1.13 fast file."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] == MAGIC_UNSIGNED:
        return read_unsigned(data, exe).zone
    return read_signed(data, exe)
