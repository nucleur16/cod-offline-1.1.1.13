"""Isolated native helper fixtures. Never launch C1 or touch a client runtime."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

C1_HASH='d8211b2bb030a055cac9cd641bf518968bff3fd58ab51c6e950465124734ba60'


def main():
    executable, c1 = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    if hashlib.sha256(c1.read_bytes()).hexdigest()!=C1_HASH:
        raise ValueError('fixture requires the reviewed exact C1 reference')
    cases=('good','badpid','leadingpid','overflowpid','badnonce','longnonce','pathnonce','prefix',
           'flagoff','badpipe','wrongserver','missingpipe','guardflush','guardcaller','guarddvar',
           'guardprefix','guardlock','badhandle','wrongfile','nullstream','flusherror',
           'multioff','wrongroute','mixedroute','receiptpartial','runtimeiat','arbitraryiat',
           'wrongimport','nullimport','guardlocktail','exchangegood','exchangerepeat','exchangebad',
           'exchangeempty','exchangeshort','exchangelong','exchangepath','exchangewrongserver',
           'exchangeguard','exchangepreexisting','exchangepartial')
    shipping=len(sys.argv)>3 and sys.argv[3]=='shipping'
    if shipping:cases=('good',)
    for case in cases:
        with tempfile.TemporaryDirectory(prefix='codol-logflush-fixture-') as folder:
            root=Path(folder);target=root/'fixture.exe';shutil.copyfile(executable,target)
            (root/'main').mkdir()
            route='127.0.0.1' if case=='wrongroute' else '127.0.0.3'
            auth='127.0.0.1' if case=='mixedroute' else route
            (root/'dwauth.ini').write_text(f'{route}\n3074\n{auth}\n3074\n3ea\n',encoding='ascii')
            result=subprocess.run([str(target),case,str(c1),'+set','codol_local_test_multi',
                                   '0' if case=='multioff' else '1'],cwd=root,capture_output=True,
                                   text=True,check=True,timeout=8)
            receipts=list((root/'main').glob('*.json'))
            if case in ('good','runtimeiat') and not shipping:
                assert len(receipts)==1
                body=json.loads(receipts[0].read_text(encoding='ascii'))
                assert set(body)=={'version','pid','nonce','prefix','flushed','c1_reference_sha256','abi'}
                assert body['version']==1 and body['flushed'] is True and type(body['pid']) is int
                assert body['nonce']=='0123456789abcdef0123456789abcdef'
                assert body['prefix']=='codol_'+body['nonce']
                assert body['c1_reference_sha256']==C1_HASH and body['abi']=='c1-d8211b2-logflush-v1'
            elif case in ('exchangegood','exchangerepeat') and not shipping:
                assert len(receipts)==2
                bodies=[json.loads(p.read_text(encoding='ascii')) for p in receipts]
                exchanged=[body for body in bodies if body['version']==2]
                assert len(exchanged)==(2 if case=='exchangerepeat' else 1)
                for body in exchanged:
                    assert set(body)=={'version','pid','nonce','prefix','flushed','c1_reference_sha256','abi','exchange'}
                    assert body['nonce']=='0123456789abcdef0123456789abcdef' and body['prefix']=='codol_'+body['nonce']
                    assert body['abi']=='c1-d8211b2-logflush-v2' and body['c1_reference_sha256']==C1_HASH
                    assert body['flushed'] is True and type(body['pid']) is int
                    assert body['exchange'] in ('1'*32,'2'*32)
            elif case=='exchangepreexisting' and not shipping:
                assert len(receipts)==1 and receipts[0].read_text(encoding='ascii')=='unchanged'
            else:assert not receipts
            print(result.stdout.strip().splitlines()[-1])
    print(f'{len(cases)} isolated log-flush cases passed; C1/native CRT not executed')


if __name__=='__main__':main()
