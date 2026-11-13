"""Independent Python/C framing vectors; no authority/game/native success claim."""
import pathlib,sys,struct,subprocess,json,hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
APP=pathlib.Path(r'E:\codol-p2p-startup-ipc-20261104')
NATIVE=pathlib.Path(r'E:\codol-p2p-native-adapter-20261104')
OUT=pathlib.Path(r'E:\codol-p2p-active-control-evidence-20261104\v2-client')
sys.path.insert(0,str(APP))
from prototypes.p2p import active_control_wire as wire

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    entries=struct.pack('<Q4sHH16s',1001,b'\x7f\0\0\x01',17701,1,b'h'*16)+struct.pack('<Q4sHH16s',1002,b'\x7f\0\0\x01',17702,2,b'g'*16)
    def bundle(seq,issued):return struct.pack('<8s16s6Q2I',b'CPRBv1\0\0',b'e'*16,1,1,seq,issued,issued+30000,1001,2,0)+entries
    boot=bundle(1,1000000);manifest=b'm'*32;nonce=b'n'*32
    hello_raw=wire.encode_hello(nonce,b's'*32,b'b'*32,manifest,boot)
    hello=wire.decode_hello(hello_raw,bootstrap_nonce=b's'*32,bootstrap_binding=b'b'*32,manifest=manifest,bootstrap_bundle=boot)
    controls=[];previous=boot
    for i in range(3):
        current=b'' if i==2 else bundle(i+2,1001000+i*1000)
        controls.append(wire.encode_control(hello,sequence=i+1,active_id=b'a'*16,handoff_id=b'h'*32,
            hard_expires_ms=1180000,path_epoch=1,path_id=b'p'*16,bundle=current,previous_bundle=previous,
            now_ms=1001000+i*1000,now_mono_ms=1000+i*1000,
            owner_deadline_ms=None if i==2 else 31000+i*1000,revoke=i==2))
        if current:previous=current
    files={'scripts.bin':b'CVSPv1\0\0'+struct.pack('<II',1,0)+manifest,'bootstrap.bin':boot,
        'nonce.bin':nonce,'hello.bin':hello_raw,'update.bin':controls[0].raw,'renew.bin':controls[1].raw,'revoke.bin':controls[2].raw,
        'ack.bin':struct.pack('<8sQ32s32siIQ',wire.ACK_MAGIC,1,nonce,controls[0].binding,1,wire.UPDATE,2)}
    for name,data in files.items():(OUT/name).write_bytes(data)
    def unix(p):return '/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
    binary=unix(OUT/'active-wire')
    command=['wsl','--exec','gcc','-m32','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-I'+unix(NATIVE/'src'),unix(ROOT/'tests/test_p2p_active_wire.c'),unix(ROOT/'src/p2p/p2p_active_wire.c'),
        unix(NATIVE/'src/mbedtls/sha256.c'),unix(NATIVE/'src/mbedtls/platform_util.c'),'-o',binary]
    built=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/'codec-build.log').write_text(built.stdout+built.stderr)
    if built.returncode:raise AssertionError(built.stderr)
    run=subprocess.run(['wsl','--exec',binary,*[unix(OUT/n) for n in ['scripts.bin','bootstrap.bin','nonce.bin','hello.bin','update.bin','renew.bin','revoke.bin','ack.bin','c-ack.bin']]],capture_output=True,text=True,encoding='utf-8',errors='replace')
    (OUT/'codec-test.log').write_text(run.stdout+run.stderr)
    if run.returncode:raise AssertionError(run.stdout+run.stderr)
    assert wire.decode_ack((OUT/'c-ack.bin').read_bytes(),hello,controls[0])
    print(run.stdout.strip());print('PASS Python decode actual C ACK')
    golden=pathlib.Path(r'E:\codol-p2p-active-control-v2-golden-20261104')
    probe=unix(OUT/'active-parent-vector')
    compile_probe=command[:]
    compile_probe[compile_probe.index(unix(ROOT/'tests/test_p2p_active_wire.c'))]=unix(ROOT/'tests/p2p_active_vector_probe.c')
    compile_probe[compile_probe.index(binary)]=probe
    compile_probe.insert(compile_probe.index(unix(ROOT/'src/p2p/p2p_active_wire.c')),unix(ROOT/'src/p2p/p2p_startup_wire.c'))
    built=subprocess.run(compile_probe,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (OUT/'parent-probe-build.log').write_text(built.stdout+built.stderr)
    if built.returncode:raise AssertionError(built.stdout+built.stderr)
    probed=subprocess.run(['wsl','--exec',probe,*[unix(golden/n) for n in
        ['request.bin','envelope.bin','hello-nonce.bin','hello.bin','update.bin','ack.bin']]],
        capture_output=True,text=True,encoding='utf-8',errors='replace')
    (OUT/'parent-probe-test.log').write_text(probed.stdout+probed.stderr)
    if probed.returncode:raise AssertionError(probed.stdout+probed.stderr)
    print(probed.stdout.strip())
    receipt=dict(okay=True,codec_output=run.stdout,probe_output=probed.stdout,
        sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
            ['src/p2p/p2p_active_wire.c','src/p2p/p2p_active_wire.h','tests/test_p2p_active_wire.c','tests/test_p2p_active_wire.py','tests/p2p_active_vector_probe.c']},
        independent_golden_receipt_sha256=hashlib.sha256((golden/'receipt.json').read_bytes()).hexdigest(),
        golden_files={name:hashlib.sha256((golden/name).read_bytes()).hexdigest() for name in ['update.bin','ack.bin','envelope.bin']})
    (OUT/'active-wire-tests.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0
if __name__=='__main__':sys.exit(main())
