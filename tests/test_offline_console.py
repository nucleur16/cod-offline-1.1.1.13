"""The console of the offline client and its command sync with the embedded match server
(docs/OFFLINE_CONSOLE_20261112.md). Source checks only: no game, no server, no window.

    python -B tests/test_offline_console.py

What fails here:
  - the console is no longer in every profile, or a diagnostics-only part of it leaked into the shipping one;
  - src/console/sh1x_cmds.h differs from what scripts/gen_sh1x_cmds.py reads out of the pinned server source
    (the submodule moved, or a command/dvar/cheat/usage string changed on the server side);
  - the console's cheat table (console.c kDevCmds) and the server's cheat set (cmd_devhelp + its dispatchers)
    are not the same names with the same usage strings;
  - the server route reaches anything but the server this client hosts.
"""
import pathlib
import re
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SERVER = ROOT / "server" / "sh1x"
CONSOLE = (ROOT / "src/console/console.c").read_text(encoding="latin-1")
ROUTE = (ROOT / "src/console/console_server.c").read_text(encoding="latin-1")
KERNEL = (ROOT / "src/kernel33.c").read_text(encoding="latin-1")
P2P = (ROOT / "src/p2p/p2p_inproc.c").read_text(encoding="latin-1")
P2P_C = (ROOT / "src/p2p/p2p.c").read_text(encoding="latin-1")
HEADER = (ROOT / "src/console/sh1x_cmds.h").read_text(encoding="ascii")
sys.path.insert(0, str(ROOT / "tools"))
import drop_codol_prefix  # noqa: E402  (the server's codol_* names as the client spells them)


def server_source(rel=None):
    """A server source file (default: the developer-cheat module, under its old or a renamed path), with the
    server's codol_* command and dvar names spelled the client's way (tools/drop_codol_prefix.py)."""
    if rel is None:
        hits = sorted((SERVER / "src").rglob("*dev_cheats.c"))
        assert len(hits) == 1, hits
        path = hits[0]
    else:
        path = SERVER / rel
    return drop_codol_prefix.server_text(path.read_text(encoding="latin-1"))


def table(text, name):
    body = text[text.index("%s[] = {" % name):]
    body = body[:body.index("\n};")]
    return re.findall(r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"', body)


def strip_block(text, start_marker):
    """text without every `#if K33_DIAGNOSTICS ... #endif` block (no nesting inside them in console.c)."""
    return re.sub(r'#if K33_DIAGNOSTICS\b.*?#endif', '', text, flags=re.S)


class ConsoleInEveryProfile(unittest.TestCase):
    def test_console_is_not_gated_on_diagnostics(self):
        m = re.search(r'#if !defined\(K33_NO_CONSOLE\)\n#if K33_DIAGNOSTICS\n#include "console/k33shot.c"[^\n]*\n#endif\n'
                      r'#include "console/console.c"\n#endif', KERNEL)
        self.assertTrue(m, "kernel33.c must include console.c in every profile and k33shot.c only with diagnostics")
        m = re.search(r'#if !defined\(K33_NO_CONSOLE\)\n\s*InstallConsole\(\);\n#if K33_DIAGNOSTICS\n\s*InstallShot\(\);\n'
                      r'#endif\n#endif', KERNEL)
        self.assertTrue(m, "InstallConsole() in every profile, InstallShot() only with diagnostics")
        self.assertNotIn("K33_DIAGNOSTICS && !defined(K33_NO_CONSOLE)", KERNEL)

    def test_build_scripts_do_not_gate_it(self):
        for script in ("scripts/build.sh", "scripts/build-profile.sh"):
            self.assertNotIn("K33_NO_CONSOLE", (ROOT / script).read_text(encoding="latin-1"), script)

    def test_shipping_profile_has_no_diagnostic_console_parts(self):
        shipping = strip_block(CONSOLE, None)
        for diagnostic in ("ShotBeforePresent(", "ShotWanted(", '"k33shot"', "k33_rconPort", "ConRconLegacy(",
                           "k33_rcon.txt", "native_server_rcon.txt"):
            self.assertNotIn(diagnostic, shipping, "%s must stay inside #if K33_DIAGNOSTICS" % diagnostic)
        # the route file has no profile switch at all and no way to another server
        self.assertNotIn("K33_DIAGNOSTICS", ROUTE)
        self.assertNotIn("K33_LOCAL_TESTING", ROUTE)

    def test_toggle_key_and_off_switch_unchanged(self):
        self.assertIn('== 0x29 && !(lp & (1 << 24))', CONSOLE)  # the key left of 1
        self.assertIn('C1RegisterInt("k33_consoleKey", VK_F1', CONSOLE)
        self.assertIn('"k33_console 0"', CONSOLE)


class GeneratedServerTable(unittest.TestCase):
    def test_header_matches_the_pinned_server_source(self):
        if not (SERVER / "src" / "g_cmds.c").is_file():
            self.fail("server/sh1x is not initialised: git submodule update --init server/sh1x")
        r = subprocess.run([sys.executable, "-B", str(ROOT / "scripts/gen_sh1x_cmds.py"), "--check"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_header_names_the_pinned_commit(self):
        pin = subprocess.run(["git", "-C", str(ROOT), "ls-tree", "HEAD", "server/sh1x"], capture_output=True,
                             text=True).stdout.split()[2]
        self.assertIn("Server submodule commit: %s" % pin, HEADER)

    def test_table_has_what_the_route_relies_on(self):
        cmds = {n for n, _ in table(HEADER, "kSh1xCmds")}
        dvars = {n for n, _ in table(HEADER, "kSh1xDvars")}
        for name in ("cmdlist", "cvarlist", "dev_cheats", "dev_clients", "dev_player",
                     "dev_clientcmd", "sh1x_status", "status"):
            self.assertIn(name, cmds)
        for name in ("sv_cheats", "sv_botStop", "devServer", "g_gravity", "scr_codol_bots"):
            self.assertIn(name, dvars)


class CheatSetsAreTheSame(unittest.TestCase):
    def setUp(self):
        body = CONSOLE[CONSOLE.index("static const ConDevCmd kDevCmds[] = {"):]
        body = body[:body.index("\n};")]
        self.client = re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*([01])\s*\}', body)
        self.assertEqual(len(self.client), body.count("{ \""), "every kDevCmds row must parse")
        self.server = table(HEADER, "kSh1xCheats")

    def test_same_names(self):
        client = [n for n, _, builtin in self.client if builtin == "0"]
        server = [n for n, _ in self.server]
        self.assertEqual(sorted(client), sorted(server),
                         "console.c kDevCmds and the server's cheat commands differ")
        self.assertEqual(client, server, "keep kDevCmds in the server's devhelp order")

    def test_same_usage(self):
        server = dict(self.server)
        for name, usage, builtin in self.client:
            if builtin == "1":
                continue
            if server[name]:
                self.assertEqual(usage, server[name], "usage of %s differs from the server's" % name)
            else:
                self.assertTrue(usage == name or usage.startswith(name + " ["),
                                "%s: the server prints no usage, so only optional arguments may be shown" % name)

    def test_server_side_matches_its_own_source(self):
        """Read straight from the server source, not through the generated header."""
        dev = server_source()
        gcmds = server_source("src/g_cmds.c")
        listed = re.search(r'\w*DevPrint\(ent,\s*"dev: ([a-z ]+)"\)', dev).group(1).split() + ["devhelp"]
        self.assertEqual([n for n, _ in self.server], listed)
        own = set(re.findall(r'Q_stricmp\(verb,\s*"([a-z]+)"\)', dev))
        game = set(re.findall(r'Q_stricmp\(\s*cmd,\s*"([a-z_]+)"\s*\)', gcmds))
        for name in listed:
            self.assertTrue(name in own or name in game, "the server lists %s but dispatches no such command" % name)
        for name, usage in (("give", "give <weapon [model]|health [n]|ammo|all>"),
                            ("take", "take <weapon|weapons|health [n]|ammo|all>")):
            self.assertIn('"usage: %s"' % usage, dev)
            self.assertEqual(dict(self.server)[name], usage)

    def test_builtins_are_not_server_cheats(self):
        builtins = [n for n, _, b in self.client if b == "1"]
        self.assertEqual(builtins, ["sv", "rcon", "k33dev"])

    def test_devset_list_is_the_servers(self):
        self.assertIn('{ "devset", g_argDevset }', CONSOLE)
        self.assertIn("g_argDevset[i] = kSh1xDevset[i].name", CONSOLE)
        self.assertNotIn("kArgDevset", CONSOLE)
        self.assertNotIn("kArgRcon", CONSOLE)
        dev = server_source()
        t = dev[dev.index("static dev_dvar_t dev_dvars[]"):]
        t = t[:t.index("};")]
        self.assertEqual([n for n, _ in table(HEADER, "kSh1xDevset")], re.findall(r'\{\s*"([A-Za-z_]+)"', t))


class ServerRoute(unittest.TestCase):
    def test_only_the_hosted_server_is_reached(self):
        self.assertIn("htonl(INADDR_LOOPBACK)", CONSOLE)
        # the route's port and password come from the host session, nothing else
        self.assertEqual(ROUTE.count("P2PConsoleHost("), 2)  # the declaration and ConSvHost
        self.assertNotIn("C1RegisterInt", ROUTE)
        self.assertNotIn("GetEnvironmentVariable", ROUTE)
        self.assertNotIn("GetProcAddress", ROUTE)  # nothing is called in, or kept from, sh1x_host.dll
        self.assertNotIn("g_pi", ROUTE)
        calls = re.findall(r'ConRconOnce\(([^;]*);', ROUTE)
        self.assertEqual(len(calls), 4)
        for call in calls:
            self.assertTrue(call.startswith("job->port, job->password"), call)

    def test_host_accessor_requires_a_live_loopback_session(self):
        fn = P2P[P2P.index("static int P2PConsoleHost("):]
        fn = fn[:fn.index("\n}\n")]
        for need in ("g_p2p.state == P2P_STARTING || g_p2p.state == P2P_READY", '"127.0.0.1"', "g_p2p.startTick",
                     "EnterCriticalSection(&g_p2pLock)", "LeaveCriticalSection(&g_p2pLock)"):
            self.assertIn(need, fn)
        for forbidden in ("g_pi", "GetProcAddress", "P2PSetState"):
            self.assertNotIn(forbidden, fn)
        self.assertIsNone(re.search(r'g_p2p\.\w+(\.\w+)*\s*=[^=]', fn), "the accessor must not write host state")

    def test_no_match_message_and_routing_rules(self):
        self.assertIn('no match running: \\"%s\\" is for the match server', CONSOLE)
        self.assertIn("no match running: %s is a command of the match server", CONSOLE)
        ex = CONSOLE[CONSOLE.index("static void ConExecute(const char *line)"):]
        # sv/rcon first, then the client's own names, then cheats, then server names
        self.assertLess(ex.index('!_stricmp(t0, "sv") || !_stricmp(t0, "rcon")'), ex.index("BOOL clientHas ="))
        self.assertIn("if (!clientHas && dev) {", ex)
        self.assertIn("} else if (!clientHas && (isSet ? svKind == 2 : svKind != 0)) {", ex)

    def test_cheats_need_the_servers_own_gate(self):
        # the host's "dev_cheats on" declares its own server a developer server first; nothing sets sv_cheats
        self.assertEqual(ROUTE.count('"devServer 1"'), 1)
        self.assertIn('!_stricmp(t0, "dev_cheats") && !_stricmp(a1, "on") && ConSvCheatsOn(seg)', CONSOLE)
        self.assertNotIn("devServer", P2P_C)  # the host's start-up is not changed
        for src in (CONSOLE, ROUTE, P2P, P2P_C):
            self.assertNotIn("sv_cheats 1\"", src)
            self.assertNotIn("+devmap", src)
        dev = server_source()
        self.assertIn('dev_server = Cvar_RegisterBool("devServer"', dev)
        self.assertIn("return cheats_on() && !lobby_match_server();", dev)


if __name__ == "__main__":
    unittest.main(verbosity=1)
