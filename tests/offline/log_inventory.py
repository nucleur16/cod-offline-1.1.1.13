#!/usr/bin/env python3
"""Inventory of every (service, task) the retail client sent, from a Python-lobby dw-capture.log.

    python log_inventory.py <dw-capture.log> [--since-line N]

Prints one row per (service, task): request count, how the Python lobby answered it (err / results
histogram, UNHANDLED count) and one example argument shape (types only, no values: the log holds
player ids and names, none of which may leave the machine).
"""
import re
import sys
from collections import Counter, defaultdict

REQ = re.compile(rb"<< mc=\d+ sig=[0-9a-f]+ service=(\d+) \(([^)]*)\) task=UChar8\((\d+)\)(?: [A-Z]+)? args=\[(.*)")
REP = re.compile(rb"TASK_REPLY svc=(\d+)\([^)]*\) task=(\d+) err=(\d+) results=(\d+)(.*)")
PUSH = re.compile(rb"-> PUSH (.{0,60})")


def shape(args):
    out = []
    for m in re.finditer(rb"'([A-Za-z0-9]+)(?:\[(\d+)\])?[(=]", args):
        out.append(m.group(1).decode() + ("[%s]" % m.group(2).decode() if m.group(2) and m.group(1) != b"Blob" else ""))
    return ",".join(out)


def main():
    path = sys.argv[1]
    since = int(sys.argv[sys.argv.index("--since-line") + 1]) if "--since-line" in sys.argv else 0
    reqs, names, shapes = Counter(), {}, defaultdict(Counter)
    reps, unh, files, pushes = defaultdict(Counter), Counter(), Counter(), Counter()
    with open(path, "rb") as fh:
        for n, line in enumerate(fh, 1):
            if n < since:
                continue
            m = REQ.search(line)
            if m:
                k = (int(m.group(1)), int(m.group(3)))
                reqs[k] += 1
                names[k[0]] = m.group(2).decode()
                shapes[k][shape(m.group(4))] += 1
                if k[0] == 10:
                    f = re.search(rb"String\(\"([^\"]*)\"\)", m.group(4))
                    if f:
                        files[(k[1], f.group(1).decode("latin1"))] += 1
                continue
            m = REP.search(line)
            if m:
                k = (int(m.group(1)), int(m.group(2)))
                reps[k]["err=%s %s" % (m.group(3).decode(), "REAL" if int(m.group(4)) else "EMPTY")] += 1
                if b"UNHANDLED" in m.group(5):
                    unh[k] += 1
                continue
            m = PUSH.search(line)
            if m:
                pushes[re.sub(rb"[0-9]+", b"N", m.group(1)).decode("latin1")] += 1
    print("svc task name reqs replies unhandled shapes")
    for k in sorted(reqs):
        top = "; ".join("%s x%d" % (s or "-", c) for s, c in shapes[k].most_common(3))
        print("%3d %3d %-16s %7d  %s  unh=%d  [%s]" % (
            k[0], k[1], names.get(k[0], "?"), reqs[k],
            " ".join("%s:%d" % kv for kv in sorted(reps[k].items())), unh[k], top))
    print("\nstorage files (task, name): count")
    for k, c in sorted(files.items(), key=lambda kv: -kv[1])[:60]:
        name = re.sub(r"\d{6,}", "<uid>", k[1])
        print("  %d %-40s %d" % (k[0], name, c))
    print("\npushes")
    for k, c in pushes.most_common(40):
        print("  %6d  %s" % (c, k))


if __name__ == "__main__":
    main()
