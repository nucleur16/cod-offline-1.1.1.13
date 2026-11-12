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
    events, cur = [], None
    with open(path, "rb") as fh:
        for n, line in enumerate(fh, 1):
            if n < lo:
                continue
            if hi and n > hi:
                break
            m = RECV.match(line)
            if m:
                cur = {"t": "send", "conn": int(m.group(2)), "want": int(m.group(3)), "hex": ""}
                events.append(cur)
                continue
            m = HEXL.match(line)
            if m and cur is not None and len(cur["hex"]) < cur["want"] * 2:
                cur["hex"] += m.group(1).replace(b" ", b"").decode()
                continue
            m = OPEN.match(line)
            if m:
                events.append({"t": "open", "conn": int(m.group(1))})
                cur = None
                continue
            m = CLOSED.match(line)
            if m:
                events.append({"t": "close", "conn": int(m.group(1))})
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

    def collect(self, quiet):
        """Read every open connection until all have been silent for `quiet` seconds.
        -> {conn: [frame, ...]} where frame = ("keepalive",) | ("plain", bytes) | ("enc", mc, plaintext)."""
        out = {}
        deadline = time.time() + quiet
        while time.time() < deadline:
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


# (svc, task) -> how many leading plaintext bytes are compared; None = everything.
# Plaintext = DEADBEEF(4) type(1) [10 txn(8)] [8 err(4)] [3 task] [8 n(4)] [8 total(4)] records...
def mask(svc, task, frames):
    out = []
    for f in frames:
        if f[0] == "enc" and (svc, task) == (12, 6) and len(f[2]) >= 37:
            pt = bytearray(f[2])
            pt[33:37] = b"TIME"          # getServerTime's Unix seconds
            out.append(("enc", f[1], bytes(pt)))
        else:
            out.append(f)
    return out


def fmt_frame(f):
    if f[0] != "enc":
        return f[0] + ("" if len(f) < 2 else " %dB %s" % (len(f[1]), f[1][:24].hex()))
    pt = f[2]
    kind = pt[4]
    return "enc mc=%d type=%d %dB %s" % (f[1], kind, len(pt), pt[5:69].hex())


def first_diff(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i
    return min(len(a), len(b))


def replay(args):
    with open(args.session) as fh:
        steps = plan(json.load(fh))
    peers = [Peer(args.a)] + ([Peer(args.b)] if args.b else [])
    total = diffs = 0
    stats = {}
    for kind, conn, data in steps:
        if kind == "open":
            for p in peers:
                p.open(conn)
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
        for p in peers:
            got.append(p.collect(args.quiet))
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
            a = {c: mask(req[0], req[1], f) if req else f for c, f in got[0].items()}
            b = {c: mask(req[0], req[1], f) if req else f for c, f in got[1].items()}
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
                            d = first_diff(xa[2], xb[2])
                            print("     first difference at plaintext byte %d: A %s | B %s" % (
                                d, xa[2][d:d + 24].hex(), xb[2][d:d + 24].hex()))
                if args.stop:
                    break
    if len(peers) == 2:
        print("\n(svc, task): requests / differing")
        for k in sorted(stats, key=str):
            print("  %-14s %5d / %d" % (k, stats[k][0], stats[k][1]))
        print("TOTAL %d requests, %d differ" % (total, diffs))
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
