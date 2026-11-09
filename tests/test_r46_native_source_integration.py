"""Portable reviewed native Source contracts; no C/API/DLL/game execution."""
from pathlib import Path
import builtins,contextlib,hashlib,io,json,re,types,unittest
ROOT=Path(__file__).resolve().parents[1]
CONTRACT=json.loads((ROOT/'tests/r46_native_source_contract.json').read_bytes())
def function(text,name):
 match=re.search(r'^[^\n]*\b'+re.escape(name)+r'\([^;{}]*\)\s*\{',text,re.M)
 if not match:raise AssertionError('function missing '+name)
 start=match.start();i=text.index('{',start);end=i+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
def pinned_function(file,name,body):return hashlib.sha256(' '.join(body.split()).encode()).hexdigest()==CONTRACT['function_whitespace_hashes'][file][name]
def gate_project(text,macro,value):
 out=[];enabled=True;stack=[]
 for line in text.splitlines(True):
  token=line.strip()
  if token.startswith('#if '):
   exact=token=='#if '+macro;stack.append((enabled,exact));enabled=enabled and (value if exact else True)
   if not exact and enabled:out.append(line)
  elif token.startswith(('#ifdef ','#ifndef ')):
   stack.append((enabled,False))
   if enabled:out.append(line)
  elif token=='#else':
   if not stack:raise AssertionError('unbalanced else')
   parent,exact=stack[-1]
   if exact:enabled=parent and not enabled
   elif enabled:out.append(line)
  elif token=='#endif':
   if not stack:raise AssertionError('unbalanced endif')
   parent,exact=stack.pop()
   if not exact and enabled:out.append(line)
   enabled=parent
  elif enabled:out.append(line)
 if stack:raise AssertionError('unclosed condition')
 return ''.join(out)
class NativeR46Source(unittest.TestCase):
 def test_profile_defaults_and_kernel_only_optin_remain(self):
  profile=(ROOT/'production_profile.h').read_text();self.assertIn('#define CODOL_LOCAL_TESTING 0',profile);self.assertIn('#define CODOL_KERNEL_ROOM_CONTEXT 0',profile);self.assertIn('#define CODOL_KERNEL_NAME_DISPLAY 0',profile)
  kernel=(ROOT/'kernel33.c').read_text();self.assertNotIn('InstallP2PInproc();',gate_project(kernel,'CODOL_LOCAL_TESTING',False));self.assertIn('InstallP2PInproc();',gate_project(kernel,'CODOL_LOCAL_TESTING',True));self.assertIn('K33OnMainTick(P2PLobbyTick)',(ROOT/'p2p.c').read_text());self.assertNotIn('Direct3DCreate9',(ROOT/'k33tick.c').read_text())
 def test_development_command_storage_fault_and_registration_all_gated(self):
  text=(ROOT/'p2p_inproc.c').read_text();edits=CONTRACT['dev_gate_sections'];self.assertEqual(len(edits),4)
  for row in edits:self.assertEqual(text.count(row['after']),1)
  shipping=gate_project(text,'CODOL_LOCAL_TESTING',False);development=gate_project(text,'CODOL_LOCAL_TESTING',True)
  for symbol in ('g_piFaultTest','PiCmdFaultTest','g_piCmds'):self.assertNotIn(symbol,shipping);self.assertIn(symbol,development)
  for row in edits:
   changed=text.replace(row['after'],row['before'],1)
   self.assertTrue(any(x in gate_project(changed,'CODOL_LOCAL_TESTING',False)for x in ('g_piFaultTest','PiCmdFaultTest','g_piCmds')))
 def test_hostgate_diagnostics_only_and_operational_function_hashes(self):
  text=(ROOT/'hostgate.c').read_text();shipping=gate_project(text,'CODOL_DIAGNOSTICS',False)
  for symbol in ('g_hgRefusedCount','g_hgDropsIn','g_hgDropsOut','InterlockedIncrement'):self.assertNotIn(symbol,shipping)
  self.assertIn('#define HostGateRefused(...) ((void)0)',shipping)
  for name in CONTRACT['function_whitespace_hashes']['hostgate.c']:self.assertTrue(pinned_function('hostgate.c',name,function(text,name)))
  body=function(text,'HostGateRecvFrom');self.assertFalse(pinned_function('hostgate.c','HostGateRecvFrom',body.replace('dropped < 64','dropped < 63',1)))
  body=function(text,'HostGateOk');mutant=body.replace('return ok;','return TRUE;',1);self.assertNotEqual(mutant,body);self.assertFalse(pinned_function('hostgate.c','HostGateOk',mutant))
 def test_ipv4_port_and_both_callers_exact_register_stack_flag_contract(self):
  text=(ROOT/'challenge_sender_guard.c').read_text()
  for name in CONTRACT['function_whitespace_hashes']['challenge_sender_guard.c']:self.assertTrue(pinned_function('challenge_sender_guard.c',name,function(text,name)))
  for name,word in (('ChallengeSenderGuard','cmpw -0xc60(%%ebp), %%ax'),('ConnectSenderCompare','cmpw 0x24(%%esp), %%ax')):
   body=function(text,name);self.assertIn(word,body);self.assertFalse(pinned_function('challenge_sender_guard.c',name,body.replace(word,'nop',1)))
 def test_patchdir_size_query_is_diagnostic_but_header_read_stays(self):
  text=(ROOT/'kernel33.c').read_text();self.assertRegex(text,r'#if CODOL_DIAGNOSTICS\s+DWORD size = GetFileSize\(f, NULL\);\s+#endif')
  shipping=gate_project(function(text,'LoadPatchDir'),'CODOL_DIAGNOSTICS',False);self.assertNotIn('GetFileSize(',shipping);self.assertIn('ReadFile(f, hdr, sizeof(hdr), &got, NULL)',shipping);self.assertIn('CloseHandle(f)',shipping)
 def test_guest_dispatch_queued_loopback_only_and_original_currentness_checks(self):
  text=(ROOT/'p2p_guest_binding.c').read_text();body=function(text,'P2PGConnectDispatchTick')
  self.assertIn('connect 127.0.0.1:%u\\n',body);self.assertEqual(body.count('C1CbufAddText(line)'),1);self.assertEqual(body.count('P2PGNativeCurrent()'),2);self.assertEqual(body.count('P2PGRoomCurrent()'),2)
  self.assertIn('connect_spent,1,0',body);self.assertIn('P2PGConnectCommandExists()',body);self.assertIn('QUEUED only',text)
  self.assertTrue(pinned_function('p2p_guest_binding.c','P2PGConnectDispatchTick',body))
  for old,new in (('P2PGNativeCurrent()','TRUE'),('P2PGRoomCurrent()','TRUE'),('connect 127.0.0.1:%u','connect 192.0.2.1:%u'),('connect_spent,1,0','connect_spent,0,0')):
   mutant=body.replace(old,new,1);self.assertNotEqual(mutant,body);self.assertFalse(pinned_function('p2p_guest_binding.c','P2PGConnectDispatchTick',mutant))
  self.assertNotIn('C1QueueSet("ui_p2p_ready","1")',text);self.assertIn('C1QueueSet("ui_p2p_ready","0")',text)
  shipping=gate_project(text,'CODOL_LOCAL_TESTING',False);self.assertNotIn('C1CbufAddText(line)',shipping);self.assertIn('P2PGuestThread(LPVOID ignored){(void)ignored;return 1;}',shipping)
 def test_guest_wire_sizes_and_bound_dispatch_not_ready_authority(self):
  header=(ROOT/'p2p_guest_binding_wire.h').read_text()
  for name,n in (('P2PG_PACKET_SIZE',608),('P2PG_ENVELOPE_SIZE',688),('P2PG_ACK_SIZE',76),('P2PG_DISPATCH_SIZE',88),('P2PG_QUEUED_SIZE',80)):self.assertRegex(header,r'#define '+name+r' '+str(n)+r'u')
  text=(ROOT/'p2p_guest_binding_wire.c').read_text();body=function(text,'P2PGuestConnectParse');self.assertIn('v->connect_dispatch!=1',body);self.assertIn('p2pg_u64(p+76)!=p2pg_u64(v->packet+88)',body);self.assertIn('p2pg_u32(p+84)',body)
  self.assertIn('x==p2pg_u16(v->packet+516)',body);self.assertIn('x<1024u||x>65535u',body)
 def test_strict_actual_CI_gate_rejects_every_missing_or_failed_new_step(self):
  workflow=(ROOT/'.github/workflows/shared-ui-baseline.yml').read_text();code=workflow.split("          python - <<'PY'\n",1)[1].split('          PY',1)[0];code='\n'.join(line[10:]for line in code.splitlines())
  complete={'native_check_%02d'%i:{'outcome':'success'}for i in range(1,19)}
  def execute(steps):
   fake=types.SimpleNamespace(environ={'NATIVE_QUALIFICATION_STEPS':json.dumps(steps)});safe=dict(vars(builtins))
   def importer(name,*a,**k):
    if name=='json':return json
    if name=='os':return fake
    raise AssertionError('unexpected CI gate import')
   safe['__import__']=importer
   with contextlib.redirect_stdout(io.StringIO()):exec(compile(code,'<real workflow gate over RAM>','exec'),{'__builtins__':safe})
  execute(complete)
  for i in range(1,19):
   trial=dict(complete);del trial['native_check_%02d'%i]
   with self.subTest(missing=i),self.assertRaises(SystemExit):execute(trial)
  for i in (16,17,18):
   for state in ('failure','skipped','cancelled'):
    trial=dict(complete);trial['native_check_%02d'%i]={'outcome':state}
    with self.subTest(check=i,state=state),self.assertRaises(SystemExit):execute(trial)

class ProductionTestSurfaces(unittest.TestCase):
 def setUp(self):
  self.kernel=(ROOT/'kernel33.c').read_text()
  self.rows=CONTRACT['development_surface_sections']
 def current_contract(self,text):
  if len(self.rows)!=2 or any(text.count(row['after'])!=1 for row in self.rows):return False
  shipping=gate_project(text,'CODOL_LOCAL_TESTING',False)
  local=gate_project(text,'CODOL_LOCAL_TESTING',True)
  return ('k33patch' not in function(shipping,'LoadPatchDir') and 'InstallK33LogFlush();' not in shipping and
          all(row['before'] in local for row in self.rows) and
          'CodolPatchLanguageFromArgv' in function(shipping,'LoadPatchDir') and
          'ReadFile(f, hdr, sizeof(hdr), &got, NULL)' in function(shipping,'LoadPatchDir') and
          'CloseHandle(f)' in function(shipping,'LoadPatchDir'))
 def test_production_omits_test_directory_probe_and_development_command_registration(self):
  self.assertTrue(self.current_contract(self.kernel))
  for flags in ('0000','0010','0011'):
   with self.subTest(profile=flags):
    shipping=gate_project(self.kernel,'CODOL_LOCAL_TESTING',False)
    self.assertNotIn('GetFileAttributesA(test)',function(shipping,'LoadPatchDir'))
    self.assertNotIn('InstallK33LogFlush();',shipping)
 def test_local_profile_retains_original_test_override_and_logflush_call(self):
  local=gate_project(self.kernel,'CODOL_LOCAL_TESTING',True)
  for row in self.rows:self.assertIn(row['before'],local)
  self.assertEqual(local.count('InstallK33LogFlush();'),1)
  self.assertIn('GetFileAttributesA(test)',function(local,'LoadPatchDir'))
 def test_ungated_baseline_and_either_reintroduced_surface_are_rejected(self):
  baseline=self.kernel
  for row in self.rows:baseline=baseline.replace(row['after'],row['before'],1)
  self.assertFalse(self.current_contract(baseline))
  for row in self.rows:
   mutant=self.kernel.replace(row['after'],row['before'],1)
   with self.subTest(surface=row['label']):self.assertFalse(self.current_contract(mutant))
  for row in self.rows:
   mutant=self.kernel.replace(row['after'],row['after'].replace('#if CODOL_LOCAL_TESTING','#if CODOL_DIAGNOSTICS',1),1)
   with self.subTest(wrong_gate=row['label']):self.assertFalse(self.current_contract(mutant))
 def test_logflush_functional_body_is_local_only_and_shipping_refuses(self):
  body=function((ROOT/'local_log_flush.c').read_text(),'K33LfExecuteExchange')
  shipping=gate_project(body,'CODOL_LOCAL_TESTING',False)
  self.assertIn('return K33LfFail(LF_SHIPPING);',shipping)
  for token in ('K33LfPolicy()', 'K33LfPipe()', 'K33LfGuard()', 'K33LfNative(expected)'):
   self.assertNotIn(token,shipping)
   self.assertIn(token,gate_project(body,'CODOL_LOCAL_TESTING',True))

if __name__=='__main__':unittest.main(verbosity=2)
