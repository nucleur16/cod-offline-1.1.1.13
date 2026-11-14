"""Portable reviewed native Source contracts; no C/API/DLL/game execution."""
from pathlib import Path
import hashlib,json,re,unittest
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
  token=re.sub(r'(?:\s*/\*(?:[^*]|\*(?!/))*\*/)+\s*$','',line.strip()).strip()
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
class GateProjectComments(unittest.TestCase):
 def test_trailing_block_comments_preserve_both_macro_projections(self):
  text=('#if CODOL_LOCAL_TESTING /* section */ /* second ** tag */\n'
        'diagnostic_only();\n'
        '#else /* section */\n'
        'shipping_only();\n'
        '#endif /* section */\n')
  self.assertEqual(gate_project(text,'CODOL_LOCAL_TESTING',False),'shipping_only();\n')
  self.assertEqual(gate_project(text,'CODOL_LOCAL_TESTING',True),'diagnostic_only();\n')
 def test_interior_comment_does_not_erase_compound_condition(self):
  text=('#if CODOL_LOCAL_TESTING /* interior */ && OTHER /* trailing */\n'
        'compound_true();\n'
        '#else\n'
        'compound_false();\n'
        '#endif\n')
  for value in (False,True):
   self.assertEqual(gate_project(text,'CODOL_LOCAL_TESTING',value),text)

class NativeR46Source(unittest.TestCase):
 def test_profile_defaults_and_kernel_only_optin_remain(self):
  profile=(ROOT/'src/production_profile.h').read_text();self.assertIn('#define CODOL_LOCAL_TESTING 0',profile);self.assertIn('#define CODOL_KERNEL_ROOM_CONTEXT 0',profile);self.assertIn('#define CODOL_KERNEL_NAME_DISPLAY 0',profile)
  kernel=(ROOT/'src/kernel33.c').read_text();shipping=gate_project(kernel,'CODOL_LOCAL_TESTING',False);local=gate_project(kernel,'CODOL_LOCAL_TESTING',True)
  self.assertNotIn('InstallP2PInproc();',shipping);self.assertNotIn('InstallP2PInproc();',local)
  self.assertNotIn('InstallP2PInprocDiagnostics();',shipping);self.assertIn('InstallP2PInprocDiagnostics();',local)
  host=(ROOT/'src/p2p/p2p.c').read_text();start=function(host,'P2PStartConfigured')
  self.assertIn('K33OnMainTick(P2PLobbyTick)',host);self.assertLess(start.index('P2PEnsureInprocActivation()'),start.index('CreateThread('))
  self.assertNotIn('Direct3DCreate9',(ROOT/'src/k33tick.c').read_text())
 def test_development_command_storage_fault_and_registration_all_gated(self):
  text=(ROOT/'src/p2p/p2p_inproc.c').read_text();edits=[dict(row)for row in CONTRACT['dev_gate_sections']];self.assertEqual(len(edits),4)
  # The SAME registration guard now surrounds the separate QA-only function.
  # Keep all four surfaces and their ungated mutation negatives, not a waiver.
  self.assertEqual(edits[-1]['label'],'diagnostic command registration')
  diagnostics=function(text,'InstallP2PInprocDiagnostics')
  self.assertIn('C1AddCommand(&g_piCmds[i])',diagnostics)
  edits[-1]=dict(edits[-1],before=diagnostics,after='#if CODOL_LOCAL_TESTING\n'+diagnostics+'\n#endif')
  for row in edits:self.assertEqual(text.count(row['after']),1)
  shipping=gate_project(text,'CODOL_LOCAL_TESTING',False);development=gate_project(text,'CODOL_LOCAL_TESTING',True)
  for symbol in ('g_piFaultTest','PiCmdFaultTest','g_piCmds'):self.assertNotIn(symbol,shipping);self.assertIn(symbol,development)
  for row in edits:
   changed=text.replace(row['after'],row['before'],1)
   self.assertTrue(any(x in gate_project(changed,'CODOL_LOCAL_TESTING',False)for x in ('g_piFaultTest','PiCmdFaultTest','g_piCmds')))
 def test_hostgate_diagnostics_only_and_operational_function_hashes(self):
  text=(ROOT/'src/p2p/hostgate.c').read_text();shipping=gate_project(text,'CODOL_DIAGNOSTICS',False)
  for symbol in ('g_hgRefusedCount','g_hgDropsIn','g_hgDropsOut','InterlockedIncrement'):self.assertNotIn(symbol,shipping)
  self.assertIn('#define HostGateRefused(...) ((void)0)',shipping)
  for name in CONTRACT['function_whitespace_hashes']['src/p2p/hostgate.c']:self.assertTrue(pinned_function('src/p2p/hostgate.c',name,function(text,name)))
  body=function(text,'HostGateRecvFrom');self.assertFalse(pinned_function('src/p2p/hostgate.c','HostGateRecvFrom',body.replace('dropped < 64','dropped < 63',1)))
  body=function(text,'HostGateOk');mutant=body.replace('return ok;','return TRUE;',1);self.assertNotEqual(mutant,body);self.assertFalse(pinned_function('src/p2p/hostgate.c','HostGateOk',mutant))
 def test_ipv4_port_and_both_callers_exact_register_stack_flag_contract(self):
  text=(ROOT/'src/guards/challenge_sender_guard.c').read_text()
  for name in CONTRACT['function_whitespace_hashes']['src/guards/challenge_sender_guard.c']:self.assertTrue(pinned_function('src/guards/challenge_sender_guard.c',name,function(text,name)))
  for name,word in (('ChallengeSenderGuard','cmpw -0xc60(%%ebp), %%ax'),('ConnectSenderCompare','cmpw 0x24(%%esp), %%ax')):
   body=function(text,name);self.assertIn(word,body);self.assertFalse(pinned_function('src/guards/challenge_sender_guard.c',name,body.replace(word,'nop',1)))
 def test_patchdir_size_query_is_diagnostic_but_header_read_stays(self):
  text=(ROOT/'src/kernel33.c').read_text();self.assertRegex(text,r'#if CODOL_DIAGNOSTICS\s+DWORD size = GetFileSize\(f, NULL\);\s+#endif')
  shipping=gate_project(function(text,'LoadPatchDir'),'CODOL_DIAGNOSTICS',False);self.assertNotIn('GetFileSize(',shipping);self.assertIn('ReadFile(f, hdr, sizeof(hdr), &got, NULL)',shipping);self.assertIn('CloseHandle(f)',shipping)
class ProductionTestSurfaces(unittest.TestCase):
 def setUp(self):
  self.kernel=(ROOT/'src/kernel33.c').read_text()
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
  body=function((ROOT/'src/local_log_flush.c').read_text(),'K33LfExecuteExchange')
  shipping=gate_project(body,'CODOL_LOCAL_TESTING',False)
  self.assertIn('return K33LfFail(LF_SHIPPING);',shipping)
  for token in ('K33LfPolicy()', 'K33LfPipe()', 'K33LfGuard()', 'K33LfNative(expected)'):
   self.assertNotIn(token,shipping)
   self.assertIn(token,gate_project(body,'CODOL_LOCAL_TESTING',True))

 def test_scoreboard_repair_keeps_functional_calls_without_shipping_diagnostic_counter(self):
  body=function((ROOT/'src/kernel33.c').read_text(),'ScoresOpenHook')
  shipping=gate_project(body,'CODOL_DIAGNOSTICS',False)
  diagnostic=gate_project(body,'CODOL_DIAGNOSTICS',True)
  digest=lambda text:hashlib.sha256(' '.join(text.split()).encode()).hexdigest()
  # Golden preimage identities pin the entire existing repair/control flow;
  # a removed/reordered UI call or changed condition cannot pass this check.
  self.assertEqual(digest(shipping),'5086067800d27c3d30fab20d6bf98390b7220ff23099b8173ad4cf3e7c47ad16')
  self.assertEqual(digest(diagnostic),'814390d3fd06c211ba5569a8211ab1dbb450ae02741e65e979c738788936fcee')
  self.assertNotIn('reloads',shipping);self.assertNotIn('InterlockedIncrement',shipping)
  self.assertEqual(diagnostic.count('InterlockedIncrement(&reloads)'),1)
  self.assertEqual(shipping.count('((UiModeFn)(DWORD_PTR)UI_MODE_VA)'),2)


 def test_parity_debug_blocks_are_diagnostic_only_and_shared_cgame_is_preserved(self):
  text=(ROOT/'src/parity.c').read_text(encoding='utf-8')
  digest=lambda value:hashlib.sha256(' '.join(value.split()).encode()).hexdigest()
  def valid(value):
   try:
    shipping=gate_project(value,'CODOL_DIAGNOSTICS',False)
    diagnostic=gate_project(value,'CODOL_DIAGNOSTICS',True)
   except AssertionError:return False
   return (value.count('#if CODOL_DIAGNOSTICS\n')==2 and
           digest(shipping)=='e77bede52cf1ca517b9b1081f03513cb1fcebfdcc286b6c0628b7d990f54b95c' and
           digest(diagnostic)=='6f84ad3127191a21fa3b3a4ecbf29c63b8e2832229e174128cce666dd00ab30f')
  self.assertTrue(valid(text))
  shipping=gate_project(text,'CODOL_DIAGNOSTICS',False)
  self.assertIn('static BYTE *ParityCg(void)',shipping)
  for symbol in ('ParityEvents','ParityPredCheck','ParitySnapshot','ParityDefaultAsset','g_phEvents','g_parityQueue','InstallParityLogger'):
   self.assertNotIn(symbol,shipping)
  # Removing either gate must reintroduce debug code; common-helper drift must also fail.
  begin='#if CODOL_DIAGNOSTICS\n';end='#endif /* CODOL_DIAGNOSTICS */\n'
  for index in (0,1):
   starts=[match.start()for match in re.finditer(re.escape(begin),text)]
   at=starts[index];stop=text.index(end,at)
   mutant=text[:at]+text[at+len(begin):stop]+text[stop+len(end):]
   with self.subTest(ungated=index):self.assertFalse(valid(mutant))
  self.assertFalse(valid(text.replace('enc ^ CG_KEY','enc | CG_KEY',1)))

if __name__=='__main__':unittest.main(verbosity=2)
