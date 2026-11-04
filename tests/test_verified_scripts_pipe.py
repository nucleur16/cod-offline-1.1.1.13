"""Run the actual Windows pipe consumer in isolated processes; no C1 or server."""
import subprocess
import sys

if __name__ == '__main__':
    cases = ('off','missing','badname','badpid','badflag','absent','selection','good','fragmented',
             'wrongpid','refusal','oversize','truncated','timeout','magic','sink','missingapi')
    if len(sys.argv)>2 and sys.argv[2] == 'shipping':cases = ('off','good')
    for case in cases:
        subprocess.run([sys.argv[1],case],check=True,timeout=8)
    print(str(len(cases))+' isolated verified-script IPC cases passed')
