"""Run the compiled offline_lobby.c fixture, once per isolated scenario, each in its own folder.

    python tests/test_offline_lobby_windows.py <offline_lobby_fixture.exe> <empty work folder>
"""
import os
import shutil
import subprocess
import sys

CASES = [
    # (case, k33_offline.ini text or None, extra client arguments, environment additions, folders to create first)
    # There is no switch: no argument and no ini at all starts the lobby (a free lobby port, the configured
    # match host port when it is free, the gates lifted, profile "Player") ...
    ("plain", None, [], {}, []),
    # ... the switch of earlier builds is not needed and not an off switch either: `enabled = 0` changes nothing
    # (the other keys of that ini still count), and `+set k33_offline 0` is not read
    ("oldswitch", "enabled = 0\nname = edtest\n", [], {}, []),
    ("plain", None, ["+set", "k33_offline", "0"], {}, []),
    ("plain", None, ["+set", "k33_offline", "1"], {}, []),
    ("on", None, ["+set", "dw_username", "edtest"], {}, []),
    ("badname", None, ["+set", "dw_username", "ed test"], {}, []),
    ("hostbusy", None, [], {}, []),
    ("legacy", None, [], {}, [os.path.join("k33_offline", "profile")]),
    # explicit ports (tests): used as they are
    ("ini3999", "# settings\nname = edtest\nport = 3999\nalt_port = 4003\nhost_port = 17999\n", [], {}, []),
    ("ini3074", "port = 3074\n", [], {}, []),
    ("keepgates", "level_gates = 1\n", [], {}, []),
    # refusals: the lobby cannot be provided, and nothing else is tried
    ("badgate", None, [], {}, []),
    ("badbytes", None, [], {}, []),
    ("nolisten", None, [], {}, []),
    ("launcher", None, [], {"CODOL_P2P_STARTUP_IPC_V1": "1"}, []),
    ("launcher", None, [], {"CODOL_P2P_PRODUCTION_PIPE": "x"}, []),
]


def main():
    exe, work = sys.argv[1], sys.argv[2]
    for i, (case, ini, extra, env_add, folders) in enumerate(CASES):
        folder = os.path.join(work, "case%d-%s" % (i, case))
        os.makedirs(folder)
        local = os.path.join(folder, os.path.basename(exe))
        shutil.copy(exe, local)
        for sub in folders:
            os.makedirs(os.path.join(folder, sub))
        if ini is not None:
            with open(os.path.join(folder, "k33_offline.ini"), "w", newline="\n") as fh:
                fh.write(ini)
        env = dict(os.environ)
        for name in ("CODOL_P2P_STARTUP_IPC_V1", "CODOL_P2P_PRODUCTION_PIPE", "CODOL_LOCAL_QA_DWAUTH_HANDLE"):
            env.pop(name, None)
        env.update(env_add)
        subprocess.run([local, case] + extra, check=True, timeout=15, env=env, cwd=folder)
        if case not in ("launcher",):
            # the players folder and the run-time folder exist; the old store folder is never created
            assert os.path.isdir(os.path.join(folder, "players")) and os.path.isdir(os.path.join(folder, "p2p", "offline"))
            assert os.path.isdir(os.path.join(folder, "k33_offline")) == (case == "legacy")
    print("%d isolated offline lobby scenarios passed" % len(CASES))


if __name__ == "__main__":
    main()
