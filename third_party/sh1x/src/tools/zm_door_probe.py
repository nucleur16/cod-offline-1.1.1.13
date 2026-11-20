"""Exercise real USE purchases on owned Zombies probes; never touches a running host."""
import importlib.util,json,subprocess,socket,secrets,time,os,shutil,hashlib,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('probe',ROOT/'tools/survival_startup_probe.py');probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
parser=argparse.ArgumentParser(__doc__);parser.add_argument('--game',type=Path,required=True);parser.add_argument('--out',type=Path,default=ROOT/'obj/zm-door-probes');parser.add_argument('--port',type=int,default=17761);args=parser.parse_args()
game=args.game.resolve();binary=ROOT/'bin/sh1x_dedrun.exe';raw=ROOT/'bin/home/codol_rawfiles';out=args.out.resolve();port=args.port
pack_spec=importlib.util.spec_from_file_location('package',ROOT/'tools/package.py');package=importlib.util.module_from_spec(pack_spec);pack_spec.loader.exec_module(package)
results=[]
for category,mapname,gt in [('zm','zm_factory_sh','zm'),('zm','zm_sumpf_sh','zm'),('zm','zm_theater_sh','zm')]:
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as guard:guard.bind(('127.0.0.1',port))
 stage=out/mapname;home=stage/'home';main=home/'main'
 if stage.exists():raise RuntimeError('Use a fresh --out path; never append stale probe evidence')
 main.mkdir(parents=True,exist_ok=True)
 package.build_pack(str(main/package.PACK))
 password=secrets.token_hex(12)
 settings={'dedicated':'2','fs_basepath':str(game),'fs_homepath':str(home),'net_ip':'127.0.0.1','net_port':str(port),'rcon_password':password,'logfile':'2','developer':'1','clientExe':str(game/'C1.exe'),'scriptPath':str(raw),'scriptOverlay':str(raw),'scriptNativeOverlay':str(raw),'admissionRequired':'0','profilePath':str(stage/'resources'),'securityKeyPath':str(stage/'securitykey.bin'),'codol_gameAdmissionsPath':str(stage/'admissions.txt'),'sv_zoneType':category,'sv_sessionMap':mapname,'g_gametype':gt,'fs_game':category,'c1Fixups':'1','c1Readers':'1','freeZoneImages':'1','scr_bots':'1' if category=='mp' else '0'}
 binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()
 command=[str(binary)]
 for k,v in settings.items():command+=['+set',k,v]
 command+=['+map',mapname]
 log=stage/'console.txt';ready=False;status='';responses=[];start=time.monotonic()
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 with log.open('wb') as stream:
  child=subprocess.Popen(command,cwd=binary.parent,stdout=stream,stderr=subprocess.STDOUT,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
  print('Owned',category,'pid',child.pid,'port',port,flush=True)
  try:
   while child.poll() is None and time.monotonic()-start<100:
    status=probe.rcon(port,password,'sh1x_status')
    if all(v in status for v in ['running=1','map='+mapname,'gametype='+gt]):ready=True;break
    time.sleep(.5)
   if ready:
    probe.rcon(port,password,'set scr_blockertest "addbot"');time.sleep(5)
    probe.rcon(port,password,'set scr_blockertest "dump"');time.sleep(1)
    qc=main/'qconsole.log';initial=qc.read_text(encoding='latin1')
    import re
    triggers=re.findall(r'blockertest:   ent (\d+) zombie_(?:door|debris) origin',initial)
    for ent in triggers[:3]:
     probe.rcon(port,password,'set scr_blockertest "buy 0 '+ent+'"');time.sleep(4)
    status=probe.rcon(port,password,'sh1x_status')
   qconsole=main/'qconsole.log';text=qconsole.read_text(encoding='latin1',errors='replace') if qconsole.exists() else log.read_text(encoding='latin1',errors='replace')
   errors=[line for line in text.splitlines() if any(x.lower() in line.lower() for x in ['script runtime error','Could not load default asset','script compile error','Sys_Error'])]
   result={'binary_sha256':binary_sha256,'binary_unchanged':binary_sha256==hashlib.sha256(binary.read_bytes()).hexdigest(),'category':category,'map':mapname,'gametype':gt,'pid':child.pid,'port':port,'ready':ready,'status':status,'responses':responses,'elapsed_seconds':round(time.monotonic()-start,2),'exit_code_before_shutdown':child.poll(),'log':str(qconsole),'errors':errors,'spawn_evidence':[line for line in text.splitlines() if any(x in line.lower() for x in ['blockertest:'])]}
   results.append(result);print(json.dumps(result),flush=True)
  finally:
   if child.poll() is None:
    probe.rcon(port,password,'quit')
    try:child.wait(timeout=5)
    except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
   results[-1]['owned_shutdown_exit_code']=child.returncode if results else child.returncode
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(not r['ready'] or r['errors'] or not r['binary_unchanged'] or any('charged 0)' in line for line in r['spawn_evidence']) or not any('charged ' in line and 'charged 0)' not in line for line in r['spawn_evidence']) for r in results))
