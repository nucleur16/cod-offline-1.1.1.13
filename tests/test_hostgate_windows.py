"""Run a previously compiled fixture, once per isolated failure mode."""
import subprocess
import sys


if __name__ == "__main__":
    for case in ("unmapped", "missing", "foreign_recv", "foreign_send",
                 "write1", "write2", "good", "packets", "loopback"):
        subprocess.run([sys.argv[1], case, "+set", "net_hostGate", "1"],
                       check=True, timeout=15)
    print("9 isolated hostgate scenarios passed")
