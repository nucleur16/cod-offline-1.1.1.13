"""Prepare the recorded native function fixture sources outside the Git tree.

Mount the verified materialized source at /src, this output at /recipe, and a
fresh output at /out in the pinned network-none GCC13 builder; run run-focused.sh.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,re
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(repo,source,output):
 repo=Path(repo).resolve();source=Path(source).resolve();output=Path(output).resolve();meta=repo/'native_profiles'
 receipt=json.loads((source/'SOURCE-RECEIPT.json').read_text());m=json.loads((meta/'manifest.json').read_text());profile=receipt['profile']
 assert m['routing'][receipt['category']]==profile
 pins=json.loads((meta/(profile+'-code-files.json')).read_text());assert sha(meta/(profile+'-code-files.json'))==receipt['source_manifest_sha256']
 assert all(sha(source/n)==h for n,h in pins.items()),'materialized source drift'
 assert not output.exists() and not output.is_relative_to(repo),'fixture output must be new/outside Git'
 output.mkdir(parents=True)
 fixture=repo/'tests/native_profiles'
 for name in ['test_pending_identity.c','test_party_native.c','test_legacy_quick.c','test_weapon_masks.c','test_startup_archives.c','test_route_endpoint.cpp']:
  shutil.copyfile(fixture/name,output/name)
 shutil.copyfile(meta/'baselines'/('donor-gate-'+profile+'.inc'),output/'donor-gate.inc')
 s=(source/'src/sv_client.c').read_text();a=s.index('#ifdef CODOL_SERVER\n    /* Check all verified slots');b=s.index('\t// force the IP key/value pair',a);(output/'candidate-gate.inc').write_text(s[a:b],newline='\n')
 if profile=='normal':
  shutil.copyfile(fixture/'test_training_bot_seat.c',output/'test_training_bot_seat.c')
  a=s.index('#ifdef CODOL_SERVER\n    /* Training admission is checked');b=s.index('\tif ( !newcl )',a)
  block=s[a:b]
  assert block.count('CODOL_TrainingAdmit(codol_verified_uid,')==1
  assert block.count('CODOL_TrainingBotSeat(codol_verified_uid)')==1
  assert block.count('SV_DropClient(cl,"silent")')==1 and block.count('if (cl->state==CS_FREE) newcl=cl;')==1
  prefix=s[:a]
  order=[prefix.rfind(n) for n in ['CODOL_NetworkAdmission(from, &admission)','codol_verified_uid = admission.uid;','challenge != NET_CookieHash(from)','if(*sv_password->string && !validpassword )']]
  assert all(n>=0 for n in order) and order==sorted(order)
  assert prefix.count('codol_verified_uid = admission.uid;')==1
  assert 'if (admitted && admission.uid && admission.uid == codol_connect_uid)\n            codol_verified_uid = admission.uid;' in prefix
  (output/'training-connect.inc').write_text(block,newline='\n')
 actor=(source/'src/codol/player/codol_actor_lifecycle.cpp').read_text();a=actor.index('static int ActorNearestWalkableNode(');b=actor.index('/* A dog embedded',a);(output/'endpoint.inc').write_text(actor[a:b],newline='\n')
 a=actor.index('static int ActorRouteWalkable(');b=actor.index('static int ActorStartBanned(',a);(output/'walkable.inc').write_text(actor[a:b],newline='\n')
 if profile=='normal':
  s=(source/'src/codol/assets/codol_asset_commands.c').read_text();a=s.index('int CODOL_LoadStartupArchives(void)');b=s.index('/* The group-1 archives',a);(output/'startup.inc').write_text(s[a:b],newline='\n')
 else:
  text=(fixture/'test_session.c').read_text();client=(source/'src/sv_client.c').read_text();a=client.index('__cdecl void SV_DropClientInternal(');b=client.index('__cdecl void SV_DropClient(',a)
  actual=re.findall(r'\b(CODOL_ResetClientInput|CODOL_MatchReportClientDropped)\(clientnum\);',client[a:b]);replacement=re.findall(r'\b(CODOL_ResetClientInput|CODOL_MatchReportClientDropped)\(\(unsigned\)\(newcl - svs.clients\)\);',client)
  assert actual==replacement==['CODOL_MatchReportClientDropped','CODOL_ResetClientInput'],'native drop/reset order changed'
  (output/'candidate.c').write_text(text,newline='\n');(output/'baseline-report.c').write_text(text,newline='\n')
  anchor='    test_zombie_rejoin_counters();';assert text.count(anchor)==1
  (output/'candidate-asan.c').write_text(text.replace(anchor,anchor+'\n    if(!CODOL_AsyncCaptureShutdown(5000))return 1;\n    return failures?1:0;'),newline='\n')
  donor=text
  for name in ['drop','replacement']:
   before='static void fixture_actual_'+name+'_order(unsigned slot)\n{\n    CODOL_MatchReportClientDropped(slot);\n    CODOL_ResetScoreStats(slot);\n}'
   after='static void fixture_actual_'+name+'_order(unsigned slot)\n{\n    CODOL_ResetScoreStats(slot);\n    CODOL_MatchReportClientDropped(slot);\n}'
   assert donor.count(before)==1;donor=donor.replace(before,after)
  (output/'baseline-drop-order.c').write_text(donor,newline='\n')
  donor_root=output/'donor-source';p=donor_root/'src/codol/stats/codol_match_report.c';p.parent.mkdir(parents=True);shutil.copyfile(source/'src/codol/stats/codol_match_report.c',p)
  subprocess.run(['git','-c','core.autocrlf=false','-c','core.eol=lf','apply','--whitespace=nowarn',str(meta/'baselines/session-report.patch')],cwd=donor_root,check=True,capture_output=True)
  assert sha(p)==m['baselines']['session_report_lf_sha256'];shutil.copyfile(p,output/'baseline-report-source.c')
 shutil.copyfile(meta/'run-focused.sh',output/'run-focused.sh')
 all_source={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()}
 (output/'source.sha256').write_text(''.join(h+'  '+n+'\n' for n,h in sorted(all_source.items())),newline='\n')
 generated={p.name:sha(p) for p in output.iterdir() if p.is_file()}
 (output/'FIXTURE-RECEIPT.json').write_text(json.dumps({'profile':profile,'materialized_source_manifest_sha256':receipt['source_manifest_sha256'],'files':generated,'limits':m['fixture_limits']},indent=2)+'\n')
 return generated
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 try:r=prepare(Path(__file__).resolve().parents[1],a.source,a.output)
 except (AssertionError,ValueError,KeyError,FileNotFoundError,subprocess.CalledProcessError) as e:p.exit(2,'REJECT: '+str(e)+'\n')
 print(json.dumps({'prepared_files':len(r),'runtime_or_publication':False}))
if __name__=='__main__':main()
