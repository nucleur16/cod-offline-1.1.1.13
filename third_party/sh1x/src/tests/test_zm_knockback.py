"""Zombies never knock a player back (T5 _zombiemode.gsc:60 + _callbackglobal.gsc:701).

C1's finishPlayerDamage (0x51be26..0x51bf22) gives a player knockback unless
the entity has FL_NO_KNOCKBACK (flags & 8) or the script passes
iDFLAGS_NO_KNOCKBACK (4); bullets (Bullet_Process 0x5069b0, dflags 0) and
melee (0x573e3b, dflags 0) pass neither, so the kick is decided by the game
script. T5's zombies set level.disable_player_damage_knockback and add the flag
to every player hit. This checks the CoDOL zombies callback does the same.
SPDX-License-Identifier: AGPL-3.0-or-later
"""

from pathlib import Path
import re
import unittest

REPO = Path(__file__).resolve().parents[1]
ZOMBIEMODE = REPO / "bin" / "home" / "codol_rawfiles" / "maps" / "_zombiemode.gsc"


def function_body(source, name):
    match = re.search(r"^" + re.escape(name) + r"\s*\([^)]*\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(name + " not found")
    depth, i = 1, match.end()
    while depth:
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
        i += 1
    return source[match.end():i - 1]


class ZombiesKnockbackTests(unittest.TestCase):
    def setUp(self):
        self.source = ZOMBIEMODE.read_text(encoding="utf-8", errors="replace")

    def test_init_disables_player_knockback(self):
        self.assertRegex(function_body(self.source, "init"),
                         r"level\.disable_player_damage_knockback\s*=\s*true\s*;")

    def test_every_player_hit_carries_no_knockback(self):
        body = function_body(self.source, "Callback_PlayerDamage")
        flag = re.search(r"iDFlags\s*=\s*iDFlags\s*\|\s*level\.iDFLAGS_NO_KNOCKBACK", body)
        finish = body.find("self finishPlayerDamage(")
        self.assertIsNotNone(flag)
        self.assertGreater(finish, flag.start(), "the flag must be set before finishPlayerDamage")
        guard = body.rfind("is_true( level.disable_player_damage_knockback )", 0, flag.start())
        self.assertGreaterEqual(guard, 0)


if __name__ == "__main__":
    unittest.main()
