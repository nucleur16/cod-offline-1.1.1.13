"""Start only owned isolated Survival probes; never change an active lobby/host."""
import argparse
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import secrets
import socket
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1]

def rcon(port,password,command):
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.settimeout(.4)
        sock.sendto(b'\xff\xff\xff\xffrcon '+password.encode()+b' '+command.encode(),('127.0.0.1',port))
        try:return sock.recv(65535).decode('latin1','replace')
        except (socket.timeout,ConnectionResetError):return ''

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--binary',type=Path,default=ROOT/'bin/sh1x_dedrun.exe')
    parser.add_argument('--out',type=Path,default=ROOT/'obj/survival_wave_probes')
    parser.add_argument('--port',type=int,default=17761)
    parser.add_argument('--maps',nargs='+',default=['mp_qadeem_sh','mp_strike_sh'])
    parser.add_argument('--start-wave',type=int,default=1)
    parser.add_argument('--exercise-seconds',type=float,default=0,help='Owned-only bot wave exercise after startup')
    parser.add_argument('--death-exercise',action='store_true',help='Kill owned wave enemies, then observe ten seconds of death hazards')
    parser.add_argument('--observe-dogs',action='store_true',help='Enable owned native actor contact telemetry')
    parser.add_argument('--no-godmode',action='store_true')
    args=parser.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    spec=importlib.util.spec_from_file_location('package',ROOT/'tools/package.py')
    package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
    binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest()
    results=[]
    for mapname in args.maps:
        if not mapname.startswith('mp_') or not all(c.isalnum() or c=='_' for c in mapname):parser.error('invalid map token')
        # Fail before any launch if another owner occupies the exact isolated port.
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as guard:guard.bind(('127.0.0.1',args.port))
        stage=args.out/mapname;home=stage/'home';main=home/'main';main.mkdir(parents=True,exist_ok=True)
        package.build_pack(str(main/package.PACK))
        raw=ROOT/'bin/home/codol_rawfiles';password=secrets.token_hex(12)
        command=[str(args.binary.resolve())]
        settings={'dedicated':'2','fs_basepath':str(args.game.resolve()),'fs_homepath':str(home.resolve()),
          'net_ip':'127.0.0.1','net_port':str(args.port),'rcon_password':password,'logfile':'2','developer':'1',
          'clientExe':str((args.game/'C1.exe').resolve()),'scriptPath':str(raw.resolve()),
          'scriptOverlay':str(raw.resolve()),'scriptNativeOverlay':str(raw.resolve()),
          'admissionRequired':'0','profilePath':str((stage/'resources').resolve()),
          'securityKeyPath':str((stage/'securitykey.bin').resolve()),'codol_gameAdmissionsPath':str((stage/'game_admissions.txt').resolve()),
          'sv_zoneType':'survival','sv_sessionMap':mapname,'g_gametype':'survival','fs_game':'survival',
          'scr_survival_start_wave':str(args.start_wave),'scr_survival_intermission':'30',
          'scr_survival_test':'1' if args.exercise_seconds else '0','c1Fixups':'1','c1Readers':'1','freeZoneImages':'1','scr_bots':'0'}
        for key,value in settings.items():command+=['+set',key,value]
        command+=['+map',mapname]
        log=stage/'console.txt';status='';start=time.monotonic();ready=False
        print('Starting owned isolated probe '+mapname+' on127.0.0.1:'+str(args.port),flush=True)
        with log.open('wb') as output:
            startupinfo=None
            if os.name=='nt':
                startupinfo=subprocess.STARTUPINFO()
                startupinfo.dwFlags|=subprocess.STARTF_USESHOWWINDOW
                startupinfo.wShowWindow=0
            process=subprocess.Popen(command,cwd=args.binary.resolve().parent,stdout=output,stderr=subprocess.STDOUT,startupinfo=startupinfo,
                                     creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            try:
                while time.monotonic()-start<100 and process.poll() is None:
                    status=rcon(args.port,password,'sh1x_status')
                    if 'gametype=survival' in status and 'running=1' in status and ('map='+mapname) in status:
                        ready=True;break
                    time.sleep(.5)
                if ready and args.exercise_seconds>0:
                    rcon(args.port,password,'set scr_survival_test_cmd "addbot"')
                    time.sleep(4)
                    if not args.no_godmode:
                        rcon(args.port,password,'set scr_survival_test_cmd "godmode 0"')
                        time.sleep(1)
                    if args.observe_dogs:rcon(args.port,password,'actorcontact on')
                    exercise_start=time.monotonic()
                    while time.monotonic()-exercise_start<args.exercise_seconds and process.poll() is None:
                        rcon(args.port,password,'set scr_survival_test_cmd "status"')
                        time.sleep(2)
                    if args.death_exercise and process.poll() is None:
                        rcon(args.port,password,'set scr_survival_test_cmd "killenemies"')
                        time.sleep(2)
                        hazard_start=time.monotonic()
                        while time.monotonic()-hazard_start<10 and process.poll() is None:
                            rcon(args.port,password,'set scr_survival_test_cmd "status"')
                            time.sleep(2)
                    if args.observe_dogs:rcon(args.port,password,'actorcontact report')
                    status=rcon(args.port,password,'sh1x_status')
                qconsole=main/'qconsole.log'
                text=log.read_text(encoding='latin1',errors='replace')
                if qconsole.is_file():text+='\n'+qconsole.read_text(encoding='latin1',errors='replace')
                death_counts=re.findall(r'survivaltest: wave \d+ alive 0 spawned ([1-9]\d*) killed ([1-9]\d*) failures \d+',text)
                death_completed=not args.death_exercise or (bool(death_counts) and 'died in wave' not in text)
                result={'binary_sha256':binary_sha256,'map':mapname,'pid':process.pid,'port':args.port,'ready':ready,'elapsed_seconds':round(time.monotonic()-start,2),
                        'start_wave':args.start_wave,'exercise_seconds':args.exercise_seconds,'death_exercise':args.death_exercise,'exit_code':process.poll(),
                        'exercise_evidence':[line for line in text.splitlines() if 'survivaltest:' in line],
                        'status':status.strip(),'log':str(qconsole.resolve()),'waves':[line for line in text.splitlines() if 'waves from ' in line],
                        'runtime_error':'script runtime error' in text,'death_exercise_completed':death_completed,
                        'missing_table_fatal':'Could not load default asset' in text}
                results.append(result)
                print(json.dumps(result),flush=True)
            finally:
                if process.poll() is None:
                    rcon(args.port,password,'quit')
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    (args.out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    return 0 if all(r['ready'] and not r['missing_table_fatal'] and not r['runtime_error'] and r['death_exercise_completed'] for r in results) else 1
if __name__=='__main__':raise SystemExit(main())
