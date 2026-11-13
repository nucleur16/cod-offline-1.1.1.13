"""Run the compiled offline_lobby.c fixture, once per isolated scenario, each in its own folder.

    python tests/test_offline_lobby_windows.py <offline_lobby_fixture.exe> <empty work folder>
"""
import os
import shutil
import subprocess
import sys

ON = ["+set", "k33_offline", "1"]
CASES = [
    # (case, k33_offline.ini text or None, extra client arguments, environment additions, folders to create first)
    ("off", None, [], {}, []),
    ("off", "enabled = 0\nname = edtest\nport = 3999\n", [], {}, []),
    # the default: a free lobby port, the configured match host port when it is free, the gates lifted
    ("on", None, ON, {}, []),
    ("on", None, ON + ["+set", "dw_username", "edtest"], {}, []),
    ("badname", None, ON + ["+set", "dw_username", "ed test"], {}, []),
    ("hostbusy", None, ON, {}, []),
    ("legacy", None, ON, {}, [os.path.join("k33_offline", "profile")]),
    # explicit ports (tests): used as they are
    ("ini3999", "# offline mode\nenabled = 1\nname = edtest\nport = 3999\nalt_port = 4003\nhost_port = 17999\n", [], {}, []),
    ("ini3074", "enabled = 1\nport = 3074\n", [], {}, []),
    ("keepgates", "enabled = 1\nlevel_gates = 1\n", [], {}, []),
    ("badgate", None, ON, {}, []),
    ("badbytes", None, ON, {}, []),
    ("nolisten", None, ON, {}, []),
    ("launcher", None, ON, {"CODOL_P2P_STARTUP_IPC_V1": "1"}, []),
    ("launcher", None, ON, {"CODOL_P2P_PRODUCTION_PIPE": "x"}, []),
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
        if case == "off":
            # a client that did not ask for offline mode creates nothing
            assert sorted(os.listdir(folder)) == sorted([os.path.basename(exe)] + (["k33_offline.ini"] if ini else [])), os.listdir(folder)
        elif case not in ("launcher",):
            # the players folder and the run-time folder exist; the old store folder is never created
            assert os.path.isdir(os.path.join(folder, "players")) and os.path.isdir(os.path.join(folder, "p2p", "offline"))
            assert os.path.isdir(os.path.join(folder, "k33_offline")) == (case == "legacy")
    print("%d isolated offline lobby scenarios passed" % len(CASES))


if __name__ == "__main__":
    main()
