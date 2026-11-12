"""Run the compiled offline_lobby.c fixture, once per isolated scenario, each in its own folder.

    python tests/test_offline_lobby_windows.py <offline_lobby_fixture.exe> <empty work folder>
"""
import os
import shutil
import subprocess
import sys

CASES = [
    # (case, k33_offline.ini text or None, extra client arguments, environment additions)
    ("off", None, [], {}),
    ("off", "enabled = 0\nname = edtest\n", [], {}),
    ("on", None, ["+set", "k33_offline", "1"], {}),
    ("on", None, ["+set", "k33_offline", "1", "+set", "dw_username", "edtest"], {}),
    ("ini", "# offline mode\nenabled = 1\nname = edtest\nport = 3999\n", [], {}),
    ("badbytes", "enabled = 1\nport = 3999\n", [], {}),
    ("nolisten", None, ["+set", "k33_offline", "1"], {}),
    ("launcher", None, ["+set", "k33_offline", "1"], {"CODOL_P2P_STARTUP_IPC_V1": "1"}),
    ("launcher", None, ["+set", "k33_offline", "1"], {"CODOL_P2P_PRODUCTION_PIPE": "x"}),
]


def main():
    exe, work = sys.argv[1], sys.argv[2]
    for i, (case, ini, extra, env_add) in enumerate(CASES):
        folder = os.path.join(work, "case%d-%s" % (i, case))
        os.makedirs(folder)
        local = os.path.join(folder, os.path.basename(exe))
        shutil.copy(exe, local)
        if ini is not None:
            with open(os.path.join(folder, "k33_offline.ini"), "w", newline="\n") as fh:
                fh.write(ini)
        env = dict(os.environ)
        for name in ("CODOL_P2P_STARTUP_IPC_V1", "CODOL_P2P_PRODUCTION_PIPE", "CODOL_LOCAL_QA_DWAUTH_HANDLE"):
            env.pop(name, None)
        env.update(env_add)
        subprocess.run([local, case] + extra, check=True, timeout=15, env=env, cwd=folder)
    print("%d isolated offline lobby scenarios passed" % len(CASES))


if __name__ == "__main__":
    main()
