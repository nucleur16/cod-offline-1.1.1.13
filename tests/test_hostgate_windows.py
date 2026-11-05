"""Run a previously compiled fixture, once per isolated failure mode."""
import subprocess
import sys


if __name__ == "__main__":
    for case in ("unmapped", "missing", "foreign_recv", "foreign_send",
                 "foreign_resolver", "write1", "write2", "write3", "good", "packets"):
        subprocess.run([sys.argv[1], case, "+set", "k33_hostGate", "1"],
                       check=True, timeout=15)
    print("10 isolated hostgate scenarios passed")
