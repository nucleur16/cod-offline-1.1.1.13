"""Exact-build static byte fixtures and harmless corrected-native-helper tests."""
import hashlib
from pathlib import Path
import subprocess
import sys

REFERENCES = {
    'd8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60',
    'a5a7b44e66ca72da785eae4319126c23491586dc88dde499e8e6c9cf4bd6a976',
}

def main():
    fixture=Path(sys.argv[1]).resolve()
    for name in sys.argv[2:]:
        reference=Path(name).resolve()
        assert hashlib.sha256(reference.read_bytes()).hexdigest() in REFERENCES
        result=subprocess.run([str(fixture),str(reference)],capture_output=True,text=True,timeout=20,check=True)
        print(result.stdout.strip())
    assert len(sys.argv)>2

if __name__=='__main__':main()
