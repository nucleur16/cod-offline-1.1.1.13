"""Stage the Zombies creator difficulty handoff over the exact r15 menu zone.

The creator passes its chosen tier to MatchController, but a successful room
creation does not publish that tier to Demonware. The in-room difficulty control
uses ChangeRoomInfo, so publish the same update after the room is created.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

import build_font_ff as ff
import gfx_font as gfx


RAWFILE = "scaleform/default/codo.gfx"
SOURCE_MOVIE_SHA256 = "596c97839296eef25596378d2467f5104d4b4b2334ad08e8c1040491697555ae"
OLD = """         this.goRoom(cRes.result,cRes.resultMsg);
      }
      
      private function bind_rankedMatchIntermission"""
NEW = """         this.goRoom(cRes.result,cRes.resultMsg);
         if(cRes.result == ResultObj.RESULT_SUCCESS && this._roomToCreate != null && this._roomToCreate.mode == MatchType.ZM)
         {
            CFunctionContainer.ChangeRoomInfo({"difficulty":int(this._roomToCreate.difficulty)},"difficulty");
         }
      }
      
      private function bind_rankedMatchIntermission"""


def run(*args: str) -> None:
    subprocess.run(args, check=True, capture_output=True, text=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="exact r15 english_ui.ff")
    parser.add_argument("--exe", type=Path, required=True, help="matching C1.exe RSA key")
    parser.add_argument("--jpexs", type=Path, required=True, help="ffdec-cli.exe")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    source = ff.rawfiles(ff.unwrap(args.source.read_bytes(), str(args.exe)))
    if set(source) != {"english_ui", RAWFILE} or source["english_ui"]:
        raise ValueError("unexpected UI zone assets")
    original = source[RAWFILE]
    if hashlib.sha256(original).hexdigest() != SOURCE_MOVIE_SHA256:
        raise ValueError("source menu differs from the measured r15 release")

    with tempfile.TemporaryDirectory(prefix="codol-zm-difficulty-") as temp:
        work = Path(temp)
        old_movie = work / "old.gfx"
        new_movie = work / "new.gfx"
        scripts = work / "scripts"
        old_movie.write_bytes(original)
        run(str(args.jpexs), "-selectclass", "codo.controllers.MatchController",
            "-export", "script", str(scripts), str(old_movie))
        controller = scripts / "scripts/codo/controllers/MatchController.as"
        text = controller.read_text(encoding="utf-8")
        if text.count(OLD) != 1 or "ChangeRoomInfo({\"difficulty\"" in text:
            raise ValueError("creator handoff source changed")
        controller.write_text(text.replace(OLD, NEW), encoding="utf-8")
        run(str(args.jpexs), "-importScript", str(old_movie), str(new_movie), str(scripts))
        updated = new_movie.read_bytes()
        a, b = gfx.read_movie(original), gfx.read_movie(updated)
        if a[0] != b[0] or len(a[1]) != len(b[1]):
            raise ValueError("menu structure changed")
        changed = [(i, old[0]) for i, (old, new) in enumerate(zip(a[1], b[1])) if old != new]
        if changed != [(6167, 82)]:
            raise ValueError(f"unexpected changed movie tags: {changed}")
        check = work / "roundtrip"
        run(str(args.jpexs), "-selectclass", "codo.controllers.MatchController",
            "-export", "script", str(check), str(new_movie))
        if "ChangeRoomInfo({\"difficulty\":int(this._roomToCreate.difficulty)},\"difficulty\")" not in (
            check / "scripts/codo/controllers/MatchController.as").read_text(encoding="utf-8"):
            raise ValueError("compiled menu lost the handoff")

    zone = ff.build_zone("english_ui", [(ff.bf.T_RAWFILE, ff.pack_rawfile_z(RAWFILE, updated))])
    if ff.rawfiles(zone)[RAWFILE] != updated:
        raise ValueError("zone roundtrip mismatch")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(ff.wrap(zone, str(args.exe)))
    if ff.rawfiles(ff.unwrap(args.output.read_bytes(), str(args.exe)))[RAWFILE] != updated:
        raise ValueError("wrapped FF roundtrip mismatch")
    print(f"Staged {args.output}: movie SHA-256 {hashlib.sha256(updated).hexdigest()}")


if __name__ == "__main__":
    main()
