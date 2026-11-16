"""The staged GSC comes from scripts/gsc (README.md, "GSC scripts"). Source and staging checks only: no game, no
server build, no install is touched (the staging goes to a temporary folder).

    python -B tests/test_gsc_staging.py

What fails here:
  - scripts/build-server.sh stages the server's bin/home/codol_rawfiles (or inits that submodule) again, or no
    longer stages through scripts/stage_server_data.py;
  - main/sh1x_rawfiles or the scripts/ of the server pack hold anything but the tracked files of scripts/gsc,
    byte for byte (a fake server checkout with a decoy codol_rawfiles tree is staged);
  - the pinned server's tools/package.py no longer packs bin/home/codol_rawfiles as scripts/ (then the pack layout
    the staging replaces has moved and scripts/stage_server_data.py has to follow it).
"""
import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SERVER = ROOT / "server" / "sh1x"
sys.path.insert(0, str(ROOT / "scripts"))
import stage_server_data as stage  # noqa: E402


def git(cwd, *args):
    return subprocess.run(["git", "-C", str(cwd), *args], check=True, capture_output=True).stdout


def tracked_gsc():
    out = git(ROOT, "ls-files", "-z", "--", "scripts/gsc").decode("utf-8")
    return {p[len("scripts/gsc/"):]: (ROOT / p).read_bytes() for p in out.split("\0") if p and (ROOT / p).is_file()}


class GscStaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gsc = tracked_gsc()

    def test_tree_is_here(self):
        self.assertIn("maps/mp/gametypes/_callbacksetup.gsc", self.gsc)
        self.assertIn("maps/mp/gametypes/_missions.gsc", self.gsc)
        self.assertGreater(len(self.gsc), 1000)

    def test_build_server_stages_from_scripts_gsc(self):
        text = (ROOT / "scripts/build-server.sh").read_text(encoding="utf-8")
        code = "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))
        self.assertIn('python scripts/stage_server_data.py --server "$SRV" --stage "$STAGE"', code)
        self.assertNotIn("codol_rawfiles", code, "the Kernel33 build must not read the server's GSC submodule")
        self.assertNotIn("stage_server_pack.py", code, "the server's pack staging packs its own GSC submodule")

    def test_staged_gsc_is_scripts_gsc(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            srv = tmp / "server"
            for rel, data in (("bin/home/main/waypoints/mp_test_wp.csv", b"0,0,0\n"),
                              ("bin/home/main/mp/botclasses.txt", b"bots\n"),
                              ("bin/home/codol_rawfiles/maps/mp/gametypes/_decoy.gsc", b"decoy\n"),
                              ("bin/home/codol_rawfiles/maps/mp/gametypes/_missions.gsc", b"old copy\n")):
                (srv / rel).parent.mkdir(parents=True, exist_ok=True)
                (srv / rel).write_bytes(data)
            subprocess.run(["git", "init", "-q", str(srv)], check=True)
            git(srv, "add", "-A")
            out = tmp / "stage"
            subprocess.run([sys.executable, "-B", str(ROOT / "scripts/stage_server_data.py"), "--server", str(srv),
                            "--stage", str(out)], check=True, capture_output=True)

            raw = out / "main" / "sh1x_rawfiles"
            staged = {p.relative_to(raw).as_posix(): p.read_bytes() for p in raw.rglob("*") if p.is_file()}
            self.assertEqual(set(staged), set(self.gsc))
            self.assertEqual([k for k in staged if staged[k] != self.gsc[k]], [])

            with zipfile.ZipFile(out / "p2p" / "home" / "main" / stage.PACK) as z:
                names = z.namelist()
                scripts = {n[len("scripts/"):]: z.read(n) for n in names if n.startswith("scripts/")}
            self.assertEqual(set(scripts), {k for k in self.gsc if k.rsplit("/", 1)[-1] not in stage.PACK_SKIP})
            self.assertEqual([k for k in scripts if scripts[k] != self.gsc[k]], [])
            self.assertNotIn("scripts/maps/mp/gametypes/_decoy.gsc", names)
            self.assertIn("waypoints/mp_test_wp.csv", names)
            self.assertIn("mp/botclasses.txt", names)

    @unittest.skipUnless((SERVER / "tools" / "package.py").is_file(), "server/sh1x is not initialised")
    def test_server_pack_layout_still_has_the_gsc_tree(self):
        pack, trees, _ = stage.server_layout(SERVER)
        self.assertEqual(pack, stage.PACK)
        self.assertIn((stage.SERVER_GSC, "scripts/"), trees)


if __name__ == "__main__":
    unittest.main()
