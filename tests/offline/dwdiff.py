#!/usr/bin/env python3
"""Differential replay: the same client byte streams against the Python lobby and the C++ port.

    dwdiff.py extract <dw-capture.log> --from-line A [--to-line B] --out session.json
        Pulls every connection's raw client bytes (the RECV hexdumps) out of a Python-lobby log.
        The result holds real player ids and names: keep it OUT of the repository.

    dwdiff.py replay <session.json> --a 127.0.0.1:3174 --b 127.0.0.1:3274 [--quiet 0.25] [-v]
        Replays the session against both servers and compares the decrypted replies and pushes,
        request by request. Exit code 0 only when nothing differs outside the masked fields.

    dwdiff.py show <session.json> --a 127.0.0.1:3174
        Replays against one server and prints what it answered.

Needs PYLOBBY_DIR (the Python lobby copy) for its Tiger tables, and pycryptodome.
"""
import argparse
import json
import os
import re
import socket
import sys
import time
import zlib

sys.path.insert(0, os.environ.get("PYLOBBY_DIR", "."))
from demonware.a22_tiger import tiger          # noqa: E402
from demonware.dw_wire import parse_values, fmt_values  # noqa: E402
from Crypto.Cipher import DES3                 # noqa: E402

KEY = bytes(range(24))
RECV = re.compile(rb"^\[(\d\d:\d\d:\d\d\.\d+)\] conn#(\d+) RECV (\d+) bytes")
OPEN = re.compile(rb"^\[[^\]]+\] conn#(\d+) OPEN")
CLOSED = re.compile(rb"^\[[^\]]+\] conn#(\d+) CLOSED")
HEXL = re.compile(rb"^    [0-9a-f]{4}  ((?:[0-9a-f]{2} ?)+)")


def extract(path, lo, hi, out):
    events, cur, base = [], None, 0
    with open(path, "rb") as fh:
        for n, line in enumerate(fh, 1):
            if n < lo:
                continue
            if hi and n > hi:
                break
            if b"=== TCP listening" in line:      # a lobby restart: connection numbers start over
                base += 100000
                cur = None
                continue
            m = RECV.match(line)
            if m:
                cur = {"t": "send", "conn": base + int(m.group(2)), "want": int(m.group(3)), "hex": ""}
                events.append(cur)
                continue
            m = HEXL.match(line)
            if m and cur is not None and len(cur["hex"]) < cur["want"] * 2:
                cur["hex"] += m.group(1).replace(b" ", b"").decode()
                continue
            m = OPEN.match(line)
            if m:
                events.append({"t": "open", "conn": base + int(m.group(1))})
                cur = None
                continue
            m = CLOSED.match(line)
            if m:
                events.append({"t": "close", "conn": base + int(m.group(1))})
                cur = None
    bad = [e for e in events if e["t"] == "send" and len(e["hex"]) != e["want"] * 2]
    for e in events:
        e.pop("want", None)
    # connections whose bytes were elided from the log cannot be replayed faithfully
    badconns = {e["conn"] for e in bad}
    events = [e for e in events if e["conn"] not in badconns]
    opened = {e["conn"] for e in events if e["t"] == "open"}
    events = [e for e in events if e["conn"] in opened]
    with open(out, "w") as fh:
        json.dump(events, fh)
    print("%d events, %d connections (%d dropped: truncated hexdump)" % (
        len(events), len(opened - badconns), len(badconns)))


def iv(mc):
    return tiger(mc.to_bytes(4, "little"))[:8]


def describe_request(payload):
    """-> (svc, task, text) of an encrypted client frame, or None."""
    if not payload or payload[0] != 1 or len(payload) < 13 or (len(payload) - 5) % 8:
        return None
    mc = int.from_bytes(payload[1:5], "little")
    pt = DES3.new(KEY, DES3.MODE_CBC, iv(mc)).decrypt(payload[5:])
    svc, body = pt[4], pt[5:]
    if len(body) < 2 or body[0] != 3:
        return None
    return svc, body[1], " ".join(fmt_values(parse_values(body, mc & 0xFF)[1:]))


class Peer:
    def __init__(self, addr):
        host, port = addr.rsplit(":", 1)
        self.addr = (host, int(port))
        self.socks, self.bufs = {}, {}

    def open(self, conn):
        s = socket.create_connection(self.addr, timeout=5)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.socks[conn], self.bufs[conn] = s, b""

    def close(self, conn):
        s = self.socks.pop(conn, None)
        if s:
            s.close()

    def send(self, conn, data):
        self.socks[conn].sendall(data)

    def collect(self, quiet, expect=None, patience=5.0):
        """Read every open connection until all have been silent for `quiet` seconds.
        -> {conn: [frame, ...]} where frame = ("keepalive",) | ("plain", bytes) | ("enc", mc, plaintext).

        `expect` names the connection a task request was just sent on: its reply is waited for (up to
        `patience` seconds) before the quiet period may end. MEASURED: the Python lobby creates its
        SQLite database inside the first storage task that carries a uid and answers that one request
        after ~360 ms, which a 250 ms quiet window reports as a missing reply plus a shifted next one."""
        out = {}
        start = time.time()
        deadline = start + quiet
        while True:
            now = time.time()
            if now >= deadline and (expect is None or expect in out or expect not in self.socks or now >= start + patience):
                break
            got = False
            for conn, s in list(self.socks.items()):
                s.settimeout(0.02)
                try:
                    data = s.recv(1 << 20)
                except socket.timeout:
                    continue
                except OSError:
                    data = b""
                if not data:
                    out.setdefault(conn, []).append(("closed",))
                    self.socks.pop(conn, None)
                    continue
                got = True
                self.bufs[conn] += data
                buf = self.bufs[conn]
                while len(buf) >= 4:
                    n = int.from_bytes(buf[:4], "little")
                    if n == 0:
                        out.setdefault(conn, []).append(("keepalive",))
                        buf = buf[4:]
                        continue
                    if len(buf) < 4 + n:
                        break
                    payload, buf = buf[4:4 + n], buf[4 + n:]
                    if payload[0] == 1:
                        mc = int.from_bytes(payload[1:5], "little")
                        pt = DES3.new(KEY, DES3.MODE_CBC, iv(mc)).decrypt(payload[5:])
                        out.setdefault(conn, []).append(("enc", mc, pt))
                    else:
                        out.setdefault(conn, []).append(("plain", payload))
                self.bufs[conn] = buf
            if got:
                deadline = time.time() + quiet
        return out


def split_frames(stream, first):
    """Client bytes -> (whole pieces to send one at a time, the incomplete rest, first)."""
    out, i = [], 0
    while i + 4 <= len(stream):
        if first and len(stream) - i >= 8:
            ver = int.from_bytes(stream[i:i + 4], "little")
            rb = int.from_bytes(stream[i + 4:i + 8], "little")
            if ver in (180, 200) and rb in (0x10000, 0x40000, 0x80000, 0x100000):
                out.append(stream[i:i + 8]); i += 8; first = False
                continue
        n = int.from_bytes(stream[i:i + 4], "little")
        if n == 0:
            out.append(stream[i:i + 4]); i += 4
            continue
        if n > 0x10000:
            out.append(stream[i:]); i = len(stream)
            break
        if i + 4 + n > len(stream):
            break
        out.append(stream[i:i + 4 + n]); i += 4 + n
        first = False
    return out, stream[i:], first


def plan(events):
    """Re-chunk every connection's stream into whole frames, keeping the cross-connection order."""
    steps, pend, first = [], {}, {}
    for e in events:
        c = e["conn"]
        if e["t"] == "open":
            steps.append(("open", c, None)); pend[c] = b""; first[c] = True
        elif e["t"] == "close":
            if pend.get(c):
                steps.append(("send", c, pend[c]))
            pend.pop(c, None)
            steps.append(("close", c, None))
        elif c in pend:
            pieces, pend[c], first[c] = split_frames(pend[c] + bytes.fromhex(e["hex"]), first[c])
            for p in pieces:
                steps.append(("send", c, p))
    return steps


def mpdata_s_view(pt):
    """A GETFILE reply carrying a rendered mpdata_s -> a comparable form, else None (compare raw).

    Both lobbies stamp `challegeUpdateTime` (inflated byte 0x170, an int of Unix seconds) with the
    second of the serve on every mpdata / mpdata_s GETFILE (progression.blob_updates), so two servers
    that read the clock either side of a second boundary differ in those 4 bytes, the CRC32 at byte 0
    and the deflate stream behind them. Only a blob that is the canonical encoding (zlib level 1 of a
    CRC-correct 32768-byte v11 / 0x096B8F57 buffer) is normalised: reply header + the inflated buffer
    with the CRC and the timestamp blanked. Anything else is compared byte for byte."""
    if len(pt) < 37 or pt[31] != 19 or pt[32] != 8:
        return None
    n = int.from_bytes(pt[33:37], "little")
    blob = bytes(pt[37:37 + n])
    if len(blob) != n:
        return None
    try:
        buf = bytearray(zlib.decompress(blob))
    except zlib.error:
        return None
    if len(buf) != 32768 or buf[4:12] != (11).to_bytes(4, "little") + (0x096B8F57).to_bytes(4, "little"):
        return None
    if zlib.compress(bytes(buf), 1) != blob or zlib.crc32(bytes(buf[4:])) != int.from_bytes(buf[0:4], "little"):
        return None
    buf[0:4] = b"CRC!"
    buf[0x170:0x174] = b"TIME"
    return bytes(pt[:33]) + b"mpdata_s, canonical, inflated:" + bytes(buf)


# (svc, task) -> how many leading plaintext bytes are compared; None = everything.
# Plaintext = DEADBEEF(4) type(1) [10 txn(8)] [8 err(4)] [3 task] [8 n(4)] [8 total(4)] records...
def mask(svc, task, frames):
    """Normalise one connection's frames for comparison: the messageCount (which only shifts when one
    side sent a push the other did not, and is reported separately) and its padding run are dropped,
    and fields that legitimately differ between two runs are blanked."""
    out = []
    for f in frames:
        if f[0] != "enc":
            out.append(f)
            continue
        mc, pt = f[1], bytearray(f[2])
        if (svc, task) == (10, 3):
            secure = mpdata_s_view(pt)
            if secure is not None:
                out.append(("enc", secure))
                continue
        cut = len(pt)
        while cut > len(pt) - 7 and cut > 5 and pt[cut - 1] == (mc & 0xFF):
            cut -= 1
        pt = pt[:cut]
        if (svc, task) == (12, 6) and len(pt) >= 37:
            pt[33:37] = b"TIME"          # getServerTime's Unix seconds
        out.append(("enc", bytes(pt)))
    return out


def calm(by_conn, conn, stale=()):
    """Drop the idle keepalives of OTHER connections. MEASURED: both servers send a bare u32 0 after 20 s
    of silence, each on its own clock, so the frame lands beside whichever request happens to be in
    flight on one side and the next one on the other. The keepalive echo on the request's own
    connection is still compared.

    `stale` = the connections this replay has sent nothing on for ~10 minutes. Both servers close a
    connection that has been silent for 600 s (dw-capture.py CONN_IDLE_CLOSE, "silent 600s -- closing";
    Server::Tick), again each on its own clock, so that close is dropped the same way. A close of any
    other connection is still compared."""
    out = {}
    for c, frames in by_conn.items():
        keep = [f for f in frames if c == conn or (f != ("keepalive",) and not (f == ("closed",) and c in stale))]
        if keep:
            out[c] = keep
    return out


_NEVER = b"\x09" + b"\xff" * 7 + b"\x7f"   # the Int64 that follows the expiry in every inventory record


def rental_skew(a, b):
    """MARKETPLACE getInventory (80/10) and getPlayerData (80/30): a RENTAL's expiry is `now + days`
    read off each server's own clock when the purchase was made, so two servers that handled the
    purchase either side of a second boundary serve expiries one second apart from then on. In an
    inventory record ([10 instance][8 item][8 qty][8 xp][8 expiry][9 7FFF..][6 0][8 0]) an expiry
    that is non-zero on both sides and at most 2 s apart is taken as equal; everything else,
    a permanent (0) against a rental included, is still compared byte for byte. -> b, reconciled."""
    out = {}
    for c, fb in b.items():
        fa = a.get(c, [])
        frames = []
        for i, f in enumerate(fb):
            x = fa[i] if i < len(fa) else None
            if f[0] == "enc" and x and x[0] == "enc" and len(x[1]) == len(f[1]) and x[1] != f[1]:
                pa, pb = x[1], bytearray(f[1])
                p = pa.find(_NEVER)
                while p >= 5:
                    if pb[p:p + 9] == _NEVER and pa[p - 5] == 8 == pb[p - 5]:
                        ea = int.from_bytes(pa[p - 4:p], "little")
                        eb = int.from_bytes(pb[p - 4:p], "little")
                        if ea and eb and abs(ea - eb) <= 2:
                            pb[p - 4:p] = pa[p - 4:p]
                    p = pa.find(_NEVER, p + 9)
                f = ("enc", bytes(pb))
            frames.append(f)
        out[c] = frames
    return out


def mcs(frames):
    return [f[1] for f in frames if f[0] == "enc"]


def fmt_frame(f):
    if f[0] != "enc":
        return f[0] + ("" if len(f) < 2 else " %dB %s" % (len(f[1]), f[1][:24].hex()))
    pt = f[-1]
    return "enc type=%d %dB %s" % (pt[4], len(pt), pt[5:69].hex())


def first_diff(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i
    return min(len(a), len(b))


def replay(args):
    with open(args.session) as fh:
        steps = plan(json.load(fh))
    peers = [Peer(args.a)] + ([Peer(args.b)] if args.b else [])
    total = diffs = mcdiffs = 0
    stats = {}
    spoke = {}   # conn -> when this replay last sent on it
    for kind, conn, data in steps:
        if kind == "open":
            for p in peers:
                p.open(conn)
            spoke[conn] = time.time()
            continue
        if kind == "close":
            for p in peers:
                p.close(conn)
            continue
        req = describe_request(data[4:]) if len(data) > 8 else None
        label = ("svc=%d task=%d %s" % (req[0], req[1], req[2][:110])) if req else (
            "keepalive" if data == b"\0\0\0\0" else "plain %dB" % len(data))
        got = []
        for p in peers:
            p.send(conn, data)
        stale = {c for c, t0 in spoke.items() if c != conn and time.time() - t0 >= 590}
        spoke[conn] = time.time()
        for p in peers:
            got.append(p.collect(args.quiet, conn if req else None))
        total += 1
        key = (req[0], req[1]) if req else label.split()[0]
        st = stats.setdefault(key, [0, 0])
        st[0] += 1
        if len(peers) == 1 or args.verbose:
            print("conn#%d >> %s" % (conn, label))
            for c, frames in sorted(got[0].items()):
                for f in frames:
                    print("     A conn#%d << %s" % (c, fmt_frame(f)))
        if len(peers) == 2:
            sv, tk = (req[0], req[1]) if req else (-1, -1)
            a = calm({c: mask(sv, tk, f) for c, f in got[0].items()}, conn, stale)
            b = calm({c: mask(sv, tk, f) for c, f in got[1].items()}, conn, stale)
            if (sv, tk) in ((80, 10), (80, 30)):
                b = rental_skew(a, b)
            if a == b and {c: mcs(f) for c, f in got[0].items()} != {c: mcs(f) for c, f in got[1].items()}:
                mcdiffs += 1
            if a != b:
                diffs += 1
                st[1] += 1
                print("DIFF conn#%d >> %s" % (conn, label))
                for c in sorted(set(a) | set(b)):
                    fa, fb = a.get(c, []), b.get(c, [])
                    for i in range(max(len(fa), len(fb))):
                        xa = fa[i] if i < len(fa) else None
                        xb = fb[i] if i < len(fb) else None
                        if xa == xb:
                            continue
                        print("   conn#%d frame %d" % (c, i))
                        print("     A: %s" % (fmt_frame(xa) if xa else "-"))
                        print("     B: %s" % (fmt_frame(xb) if xb else "-"))
                        if xa and xb and xa[0] == xb[0] == "enc":
                            d = first_diff(xa[1], xb[1])
                            print("     first difference at plaintext byte %d: A %s | B %s" % (
                                d, xa[1][max(0, d - 8):d + 32].hex(), xb[1][max(0, d - 8):d + 32].hex()))
                if args.stop:
                    break
    if len(peers) == 2:
        print("\n(svc, task): requests / differing")
        for k in sorted(stats, key=str):
            print("  %-14s %5d / %d" % (k, stats[k][0], stats[k][1]))
        print("TOTAL %d requests, %d differ (%d more identical except for the messageCount)" % (
            total, diffs, mcdiffs))
        return 1 if diffs else 0
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    e = sub.add_parser("extract")
    e.add_argument("log"); e.add_argument("--from-line", type=int, default=1)
    e.add_argument("--to-line", type=int, default=0); e.add_argument("--out", required=True)
    for name in ("replay", "show"):
        r = sub.add_parser(name)
        r.add_argument("session"); r.add_argument("--a", required=True)
        r.add_argument("--b", default=None); r.add_argument("--quiet", type=float, default=0.25)
        r.add_argument("-v", "--verbose", action="store_true")
        r.add_argument("--stop", action="store_true", help="stop at the first difference")
    a = ap.parse_args()
    if a.cmd == "extract":
        extract(a.log, a.from_line, a.to_line, a.out)
        return 0
    return replay(a)


if __name__ == "__main__":
    sys.exit(main())
