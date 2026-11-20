#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Several synthetic encrypted human clients against an isolated native server.

Each client runs the same entry sequence as tools/transport_probe.py
(encrypted handshake, challenge, connect, 27 stats chunks, gamestate, userinfo,
cp) and then stays in the match: it sends a usermove every 50 ms, acknowledges
every server message and reliable command, follows map changes (loadingnewmap,
then a new gamestate on the stale serverId) and records everything that would
end the session -- an out-of-band disconnect, a snapshot gap, a lost gamestate.

It never decodes snapshot contents; it is a load and lifecycle test for the
server's per-client message paths, not a test of what a real C1 renders.

Run it only against a probe server started for the purpose (admission off,
its own test key file and rcon password). Example:

    python tools/multiclient_soak.py --reference <alpha0022>/KNOWN-GOOD-2026-08-21-movement \
        --key-file %TEMP%/soak/key.bin --port 17760 --clients 4 --duration 60 \
        --join-interval 3 --load-delay 4 --rcon-password test --rcon-at 20 "map mp_highrise"
"""
import argparse
import json
import secrets
import socket
import struct
import sys
import threading
import time
import zlib
from pathlib import Path

OOB = b'\xff\xff\xff\xff'
OP_SERVER_COMMAND, OP_GAMESTATE, OP_SNAPSHOT = 1, 2, 0
CLC_MOVE, CLC_MOVENODELTA, CLC_COMMAND, CLC_EOF = 0, 1, 2, 3


class Bits:
    """WriteBits/WriteData: a bit-pool byte is reserved at the byte
    cursor when the pool is empty; whole-byte fields never align it."""

    def __init__(self):
        self.data = bytearray()
        self.bit = 0

    def bits(self, value, count):
        for i in range(count):
            if not self.bit & 7:
                self.bit = len(self.data) * 8
                self.data.append(0)
            self.data[self.bit >> 3] |= ((value >> i) & 1) << (self.bit & 7)
            self.bit += 1

    def raw(self, data):
        self.data += data

    def long(self, value):
        self.raw(struct.pack('<I', value & 0xffffffff))


def usercmd_key(ack, checksum, command):
    """UsercmdKey (src/online/player/usercmd.c)."""
    value = 0
    for i, c in enumerate(command[:32]):
        if c == 0:
            break
        if c & 128 or c == 0x25:
            c = 0x2e
        value = (value + c * (119 + i)) & 0xffffffff
    return (ack ^ checksum ^ value ^ (value >> 10) ^ (value >> 20)) & 0xffffffff


class Client(threading.Thread):
    def __init__(self, args, index, crypto, netchan_xor, log):
        super().__init__(daemon=True)
        self.a, self.index, self.crypto, self.xor, self.log = args, index, crypto, netchan_xor, log
        self.uid = secrets.token_hex(8)
        self.name = 'Soak%d' % (index + 1)
        self.stats = {'name': self.name, 'uid': self.uid, 'snapshots': 0, 'gamestates': 0,
                      'oob': [], 'dropped': None, 'max_message': 0, 'max_gap_ms': 0,
                      'gaps_over_1s': 0, 'commands': 0, 'active_seconds': 0.0,
                      'maps': [], 'errors': []}
        self.stop = threading.Event()
        self.active_since = self.last_snapshot_at = None

    # -- transport -----------------------------------------------------------
    def send(self, payload):
        self.sock.sendto(self.crypto.build_data(payload, self.next_send, self.local_tag, self.key),
                         self.endpoint)
        self.next_send += 1

    def recv(self, timeout):
        self.sock.settimeout(timeout)
        try:
            packet, address = self.sock.recvfrom(65535)
        except socket.timeout:
            return None
        if address != self.endpoint or packet[:2] != b'\x06\x02':
            return None
        count, tag, plain, valid = self.crypto.decrypt_data(packet, self.key, self.last_receive)
        if not valid or tag != self.peer_tag or count <= self.last_receive:
            self.stats['errors'].append('bad record')
            return None
        self.last_receive = count
        return plain

    def handshake(self):
        c = self.crypto
        from Crypto.PublicKey import ECC
        self.endpoint = (self.a.host, self.a.port)
        sid = bytes([70, 0, 0, 0, 0, 0, 0, 0])
        self.peer_tag = secrets.randbelow(65535) + 1
        private = int(ECC.generate(curve='P-224').d)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 0))
        self.qport = self.sock.getsockname()[1]
        self.sock.settimeout(1)
        init = struct.pack('<BBHHH', 1, 2, 0, 0, self.peer_tag) + sid
        deadline = time.monotonic() + 30
        while True:
            self.sock.sendto(init, self.endpoint)
            try:
                ack, address = self.sock.recvfrom(65535)
                if address == self.endpoint and len(ack) == 38 and ack[:2] == b'\x02\x02':
                    break
            except socket.timeout:
                if time.monotonic() >= deadline:
                    raise AssertionError('INIT_ACK timeout')
        self.local_tag = int.from_bytes(ack[16:18], 'little')
        echo = struct.pack('<BBHH', 3, 2, self.local_tag, 0) + ack + bytes(37) + sid + c.export_pubkey(private)
        self.sock.sendto(echo, self.endpoint)
        while True:
            cookie, _ = self.sock.recvfrom(65535)
            if cookie != ack:
                break
        assert len(cookie) == 114, 'COOKIE_ACK'
        self.key, _ = c.shared_key20(cookie[6:106], self.a.security, private)
        self.last_receive = 0
        self.next_send = 1

    def oob(self, prefix):
        end = time.monotonic() + 10
        while time.monotonic() < end:
            plain = self.recv(1)
            if plain and plain.startswith(OOB + prefix):
                return plain[4:].rstrip(b'\0')
        raise AssertionError('no %r reply' % prefix)

    def connect(self):
        self.send(OOB + b'getchallenge 0123456789abcdef\0')
        self.challenge = int(self.oob(b'challengeResponse ').split()[1])
        info = ('\\name\\%s\\xuid\\%s\\2490\\-1157214325\\challenge\\%d\\qport\\%d\\rate\\25000\\snaps\\20'
                % (self.name, self.uid, self.challenge, self.qport))
        self.userinfo = info
        self.send(OOB + ('connect %s "%s"' % (self.uid, info)).encode() + b'\0')
        self.oob(b'connectResponse ')
        if self.a.valid_profile:
            # A zeroed mp/playerdata.def record (version 220, checksum
            # 0xe4fd5218, as transport_probe.py --class-perk-fixture):
            # getPlayerData/setPlayerData then work, so the client goes
            # through the gametype's connect and spawn scripts like a C1.
            profile = bytearray(32768)
            struct.pack_into('<II', profile, 4, 220, 0xe4fd5218)
            struct.pack_into('<I', profile, 0, zlib.crc32(profile[4:]))
            profile = bytes(profile)
        else:
            profile = bytes((i * 17 + self.index + i // 1240) & 255 for i in range(32768))
        for index in reversed(range(27)):
            self.send(OOB + b'stats\0' + struct.pack('<HB', self.qport, index) +
                      profile[index * 1240:(index + 1) * 1240])
            self.oob(b'statResponse ')

    # -- netchan -------------------------------------------------------------
    def reset_channel(self):
        self.out_seq = 1
        self.server_id = 255
        self.message_ack = 0
        self.server_cmd_seq = 0
        self.server_cmds = {}
        self.client_cmds = {}
        self.client_cmd_seq = 0
        self.client_cmd_acked = 0
        self.fragments = {}
        self.feed = 0
        self.cmd_time = 100
        self.active_since = None
        self.moves_after = None
        self.need_cp_after = 0

    def queue_command(self, text):
        self.client_cmd_seq += 1
        self.client_cmds[self.client_cmd_seq] = text.encode('latin1')
        return self.client_cmd_seq

    def cp_command(self):
        catalog = json.loads(self.a.pure_catalog.read_text())
        checksums = []
        for row in catalog['archives']:
            if row['archive'].startswith('localized_'):
                continue
            zeros = bytes(4 * row['entry_count'])
            checksums.append(row['client_checksum'] ^ zlib.crc32(zeros, self.feed) ^ zlib.crc32(zeros))
        aggregate = self.feed ^ len(checksums)
        for value in checksums:
            aggregate ^= value
        signed = lambda v: str(v if v < 0x80000000 else v - 0x100000000)
        return 'cp @ ' + ' '.join(map(signed, checksums + [aggregate]))

    def send_packet(self, now):
        body = Bits()
        for seq in range(self.client_cmd_acked + 1, self.client_cmd_seq + 1):
            body.bits(CLC_COMMAND, 3)
            body.long(seq)
            body.raw(self.client_cmds[seq] + b'\0')
        server_text = self.server_cmds.get(self.server_cmd_seq, b'')
        moving = (self.server_id != 255 and self.moves_after is not None and now >= self.moves_after and
                  self.client_cmd_acked >= self.need_cp_after)
        if moving:
            body.bits(CLC_MOVE if self.stats['snapshots'] else CLC_MOVENODELTA, 3)
            body.raw(b'\x01')
            key = usercmd_key(self.message_ack, self.feed, server_text)
            self.cmd_time = max(self.cmd_time + 1, self.last_snap_time + 50)
            body.bits(0, 1)
            body.long(self.cmd_time)
            body.bits(key & 1, 1)          # unchanged from the delta base
            body.bits(7, 3)                # path tag A
            body.raw(struct.pack('<fffI', 0.0, 0.0, 0.0, self.cmd_time))
        body.bits(CLC_EOF, 3)
        data = self.xor(bytes(body.data), (self.challenge ^ self.server_id ^ self.message_ack) & 255,
                        server_text)
        header = struct.pack('<IHBII', self.out_seq, self.qport, self.server_id & 255,
                             self.message_ack, self.server_cmd_seq)
        self.out_seq += 1
        self.send(header + data)

    def parse_gamestate(self, message, cursor, sequence):
        self.server_cmd_seq = max(self.server_cmd_seq, int.from_bytes(message[cursor + 1:cursor + 5], 'little'))
        cursor += 5
        stop = message.index(b'\0', cursor)
        mapname = message[cursor:stop].decode('ascii')
        cursor = message.index(b'\0', stop + 1) + 5
        assert message[cursor] == 3, 'gamestate configstring opcode'
        entries = int.from_bytes(message[cursor + 1:cursor + 3], 'little')
        cursor += 3
        state = {'bit': 0, 'cursor': cursor}

        def bits(count):
            value = 0
            for b in range(count):
                if state['bit'] % 8 == 0:
                    state['bit'] = state['cursor'] * 8
                    state['cursor'] += 1
                value |= ((message[state['bit'] // 8] >> (state['bit'] % 8)) & 1) << b
                state['bit'] += 1
            return value

        configstrings, previous = {}, -1
        for _ in range(entries):
            index = previous + 1 if bits(1) else bits(13)
            stop = message.index(b'\0', state['cursor'])
            configstrings[index] = message[state['cursor']:stop].decode('latin1')
            state['cursor'] = stop + 1
            previous = index
        cursor = state['cursor']
        assert message[cursor] == 5, 'gamestate tail'
        _, self.feed = struct.unpack_from('<II', message, cursor + 1)
        self.server_id = int(configstrings[3])
        self.message_ack = sequence
        self.stats['gamestates'] += 1
        self.stats['maps'].append(mapname)
        self.log('%s gamestate %d: %s (%d configstrings, %d bytes)' %
                 (self.name, self.stats['gamestates'], mapname, entries, len(message)))
        # Loading time of a real client; cp follows the load, then the moves.
        if self.stats['gamestates'] == 1:
            self.queue_command('userinfo "%s"' % self.userinfo)
        self.need_cp_after = self.queue_command(self.cp_command())
        self.moves_after = time.monotonic() + self.a.load_delay
        self.last_snapshot_at = None

    def handle(self, plain):
        sequence = int.from_bytes(plain[:4], 'little')
        if sequence & 0x80000000:
            index, last, stride, size = struct.unpack_from('<BBHH', plain, 4)
            sequence &= 0x1fffffff
            group = self.fragments.setdefault(sequence, {'last': last, 'chunks': {}})
            group['chunks'][index] = plain[10:]
            if len(group['chunks']) != group['last'] + 1:
                return
            message = b''.join(group['chunks'][i] for i in range(group['last'] + 1))
            del self.fragments[sequence]
        else:
            message = plain[4:]
        if len(message) < 5 or sequence <= self.message_ack:
            return
        self.stats['max_message'] = max(self.stats['max_message'], len(message))
        ack = int.from_bytes(message[:4], 'little')
        message = message[:4] + self.xor(message[4:], (self.challenge ^ sequence) & 255,
                                         self.client_cmds.get(ack, b''))
        if ack > self.client_cmd_acked and ack <= self.client_cmd_seq:
            self.client_cmd_acked = ack
        cursor = 4
        while cursor < len(message) and message[cursor] == OP_SERVER_COMMAND:
            cmdseq = int.from_bytes(message[cursor + 1:cursor + 5], 'little')
            stop = message.index(b'\0', cursor + 5)
            text = message[cursor + 5:stop]
            if cmdseq > self.server_cmd_seq:
                self.server_cmds[cmdseq] = text
                self.server_cmd_seq = cmdseq
                self.stats['commands'] += 1
            cursor = stop + 1
        if cursor < len(message) and message[cursor] == OP_GAMESTATE:
            self.parse_gamestate(message, cursor, sequence)
            return
        self.message_ack = sequence
        if cursor + 5 <= len(message) and message[cursor] == OP_SNAPSHOT:
            now = time.monotonic()
            self.last_snap_time = int.from_bytes(message[cursor + 1:cursor + 5], 'little')
            if self.last_snapshot_at is not None:
                gap = int((now - self.last_snapshot_at) * 1000)
                self.stats['max_gap_ms'] = max(self.stats['max_gap_ms'], gap)
                if gap > 1000:
                    self.stats['gaps_over_1s'] += 1
                    self.log('%s snapshot gap %d ms' % (self.name, gap))
            else:
                self.active_since = now
            self.last_snapshot_at = now
            self.stats['snapshots'] += 1

    def run(self):
        try:
            time.sleep(self.a.join_interval * self.index)
            self.handshake()
            self.connect()
            self.reset_channel()
            self.last_snap_time = 0
            self.last_snapshot_at = None
            self.log('%s connected (xuid %s)' % (self.name, self.uid))
            end = time.monotonic() + self.a.duration - self.a.join_interval * self.index
            next_send = next_score = 0
            # --client-command-at: seconds after this client connected.
            pending = sorted((float(t), c) for t, c in self.a.client_command_at)
            connected_at = time.monotonic()
            while time.monotonic() < end and not self.stop.is_set():
                now = time.monotonic()
                # The scoreboard request C1 sends while the score is shown.
                if (self.a.score_interval and self.last_snapshot_at is not None and now >= next_score and
                        self.client_cmd_acked == self.client_cmd_seq):
                    self.queue_command('s')
                    next_score = now + self.a.score_interval
                while pending and self.server_id != 255 and now - connected_at >= pending[0][0]:
                    command = pending.pop(0)[1].replace('{serverid}', str(self.server_id))
                    self.queue_command(command)
                    self.log('%s sent %r' % (self.name, command))
                if now >= next_send:
                    self.send_packet(now)
                    next_send = now + 0.05
                plain = self.recv(max(0.001, next_send - time.monotonic()))
                if plain is None:
                    continue
                if plain.startswith(OOB):
                    text = plain[4:].rstrip(b'\0').decode('latin1', 'replace')
                    self.stats['oob'].append(text[:80])
                    self.log('%s OOB: %s' % (self.name, text[:80].replace('\n', ' | ')))
                    if text.startswith('disconnect') or text.startswith('error'):
                        self.stats['dropped'] = text[:120]
                        return
                    continue
                self.handle(plain)
            if not self.stats['dropped'] and getattr(self, 'challenge', None) is not None:
                # Leave like C1 does, so the slot is free for the next run.
                self.queue_command('disconnect')
                for _ in range(3):
                    self.send_packet(time.monotonic())
                    time.sleep(0.05)
        except Exception as exc:                                   # noqa: BLE001
            self.stats['errors'].append(repr(exc))
            self.log('%s failed: %r' % (self.name, exc))
        finally:
            if self.active_since is not None and self.last_snapshot_at is not None:
                self.stats['active_seconds'] = round(self.last_snapshot_at - self.active_since, 1)


def rcon(args, command):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(3)
    s.sendto(OOB + ('rcon "%s" %s' % (args.rcon_password, command)).encode('ascii'), (args.host, args.port))
    try:
        reply, _ = s.recvfrom(65535)
        return reply[4:].decode('latin1', 'replace')
    except socket.timeout:
        return ''
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--reference', type=Path, required=True, help='alpha0022 crypto reference (dtls_crypto, netchan_ack)')
    ap.add_argument('--key-file', type=Path, required=True, help="the probe server's 16-byte securityKeyPath")
    ap.add_argument('--pure-catalog', type=Path, default=Path(__file__).resolve().parents[1] / 'docs/IWD_CLIENT_CHECKSUMS.json')
    ap.add_argument('--host', default='127.0.0.1')
    ap.add_argument('--port', type=int, required=True)
    ap.add_argument('--clients', type=int, default=2)
    ap.add_argument('--duration', type=float, default=60)
    ap.add_argument('--join-interval', type=float, default=2, help='seconds between client joins')
    ap.add_argument('--load-delay', type=float, default=2, help='seconds a client "loads" after each gamestate before moving')
    ap.add_argument('--score-interval', type=float, default=2, help='seconds between scoreboard requests (0: never)')
    ap.add_argument('--rcon-password')
    ap.add_argument('--rcon-at', nargs=2, action='append', default=[], metavar=('SECONDS', 'COMMAND'),
                    help='send an rcon command at this time (e.g. 20 "map mp_highrise")')
    ap.add_argument('--client-command-at', nargs=2, action='append', default=[], metavar=('SECONDS', 'COMMAND'),
                    help='send a client command that many seconds after connecting, e.g. '
                         '10 "mr {serverid} 16 autoassign" (join a team, never pick a class)')
    ap.add_argument('--valid-profile', action='store_true',
                    help='upload a zeroed profile that matches mp/playerdata.def instead of a byte pattern')
    ap.add_argument('--allow-drop', action='store_true', help='exit 0 even when a client was dropped')
    args = ap.parse_args()
    args.security = args.key_file.read_bytes()
    assert len(args.security) == 16, 'test key must be 16 bytes'
    sys.path.insert(0, str(args.reference.resolve()))
    # dtls_crypto imports its Tiger hash from a22_tiger, which lives in the
    # lobby's demonware package beside the KNOWN-GOOD reference.
    if (args.reference.parent / 'demonware' / 'a22_tiger.py').exists():
        sys.path.insert(0, str(args.reference.parent.resolve()))
    import dtls_crypto
    if not getattr(dtls_crypto, 'TIGER_OK', True):
        from demonware.a22_tiger import tiger
        dtls_crypto._tiger = tiger
    from netchan_ack import netchan_xor
    lock = threading.Lock()
    start = time.monotonic()

    def log(text):
        with lock:
            print('[%6.1f] %s' % (time.monotonic() - start, text), flush=True)

    clients = [Client(args, i, dtls_crypto, netchan_xor, log) for i in range(args.clients)]
    for c in clients:
        c.start()
    for at, command in sorted(args.rcon_at, key=lambda r: float(r[0])):
        delay = start + float(at) - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        log('rcon %s -> %s' % (command, rcon(args, command).strip().replace('\n', ' | ')[:120]))
    for c in clients:
        c.join(args.duration + 60)
    summary = [c.stats for c in clients]
    print(json.dumps(summary, indent=1))
    bad = [s for s in summary if s['dropped'] or s['errors'] or not s['snapshots']]
    print('RESULT: %d client(s), %d dropped, %d with errors, %d without snapshots' % (
        len(summary), sum(bool(s['dropped']) for s in summary), sum(bool(s['errors']) for s in summary),
        sum(not s['snapshots'] for s in summary)))
    return 0 if not bad or args.allow_drop else 1


if __name__ == '__main__':
    sys.exit(main())
