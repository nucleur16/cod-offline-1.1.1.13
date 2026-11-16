"""Pack the sh1x match server (the server repo's host-dll target, sh1x_host.dll) for Kernel33.dll's resources.

    python scripts/pack_server.py <sh1x_host.dll> <out.k33s> [server commit]

scripts/build.sh runs it and embeds the result as RCDATA 2 (src/p2p/p2p_embedded.c reads it). Layout, little endian:

    0   u32  magic "K33S" (0x5333334B)
    4   u32  format 1
    8   u32  size of the DLL
    12  u32  size of the zlib stream
    16  32   SHA-256 of the DLL
    48  40   server commit, ASCII, NUL padded (informational: logged when a session starts)
    88  ...  zlib stream of the DLL
"""
import hashlib
import struct
import sys
import zlib

MAGIC = 0x5333334B
FORMAT = 1


def main(argv):
    if len(argv) not in (3, 4):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    src, dst = argv[1], argv[2]
    commit = (argv[3] if len(argv) == 4 else "").encode("ascii", "replace")[:40]
    with open(src, "rb") as f:
        dll = f.read()
    if dll[:2] != b"MZ":
        print(f"pack_server: {src} is not a PE image", file=sys.stderr)
        return 1
    z = zlib.compress(dll, 9)
    if zlib.decompress(z) != dll:
        print("pack_server: zlib round trip failed", file=sys.stderr)
        return 1
    head = struct.pack("<IIII", MAGIC, FORMAT, len(dll), len(z)) + hashlib.sha256(dll).digest() + commit.ljust(40, b"\0")
    with open(dst, "wb") as f:
        f.write(head + z)
    print(f"pack_server: {src} {len(dll)} bytes -> {dst} {len(head) + len(z)} bytes, sha256 {hashlib.sha256(dll).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
