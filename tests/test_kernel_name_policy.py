"""Portable compiled C display-name policy versus an independent JSON/UTF8 oracle."""
from pathlib import Path
import argparse,ctypes,hashlib,json,random,time,unittest
if __name__!='__main__':raise unittest.SkipTest('Explicit compiled portable name-policy library required')
ap=argparse.ArgumentParser();ap.add_argument('--library',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
args,remaining=ap.parse_known_args();lib=ctypes.CDLL(str(args.library))
lib.Name.argtypes=[ctypes.c_char_p];lib.Response.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,ctypes.c_uint64,ctypes.c_void_p]
lib.Identity.argtypes=[ctypes.c_char_p,ctypes.c_uint64,ctypes.c_uint32,ctypes.c_uint32];lib.Generation.restype=ctypes.c_uint64
lib.Reply.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_uint32];lib.Alias.argtypes=[ctypes.c_char_p,ctypes.c_uint64,ctypes.c_uint32,ctypes.c_void_p]
COUNTS={'oracle_cases':0,'truncations':0,'cache_controls':0}
def response(body,canonical=b'CanonicalUser',uid=18446744073709551615):
    out=ctypes.create_string_buffer(64);result=lib.Response(body,len(body),canonical,uid,out)
    return bool(result),out.value
def alias(canonical=b'CanonicalUser',uid=7,now=100):
    out=ctypes.create_string_buffer(64);result=lib.Alias(canonical,uid,now,out);COUNTS['cache_controls']+=1
    return bool(result),out.value
def oracle(value,uid=18446744073709551615):
    if not isinstance(value,dict) or value.get('identity')!='CanonicalUser' or 'uid' not in value:return False
    if value['uid'] is not None and (not isinstance(value['uid'],str) or value['uid']!=str(uid)):return False
    name=value.get('name')
    if not isinstance(name,str) or not name or name==str(uid) or any(ord(c)<32 or ord(c)==127 for c in name):return False
    try:return len(name.encode('utf8'))<=63
    except UnicodeEncodeError:return False

class Policy(unittest.TestCase):
 def test_regular_unicode_responses_match_independent_json_oracle(self):
  rng=random.Random(0xC0D046);characters=list('Alpha beta_[]123')+['中','文','é','😀','\n','\x1b','\x7f','\ud800']
  uid=18446744073709551615
  for case in range(10000):
   name=''.join(rng.choice(characters) for _ in range(rng.randrange(41)))
   value={'identity':rng.choice(['CanonicalUser']*5+['Different','CanonicalUser\0other']),
          'uid':rng.choice([None,str(uid),str(uid),'wrong',7,True]),'name':name,
          'metadata':{'ignored':[None,False,17,-3.25e10,{'nested':'value\nwith escapes'}]}}
   if case%53==0:value.pop('uid')
   ensure_ascii=bool(case&1)
   try:body=json.dumps(value,ensure_ascii=ensure_ascii,separators=(',',':')).encode('utf8')
   except UnicodeEncodeError:body=json.dumps(value,ensure_ascii=True,separators=(',',':')).encode()
   expected=oracle(value);actual,text=response(body);COUNTS['oracle_cases']+=1
   self.assertEqual(actual,expected,(case,body))
   if expected:self.assertEqual(text,name.encode('utf8'))
 def test_exact_byte_limits_controls_invalid_utf8_and_surrogate_sequences(self):
  for name in ['A'*63,'中'*21,'😀'*15+'abc']:
   self.assertEqual(response(json.dumps({'identity':'CanonicalUser','uid':None,'name':name}).encode())[0],True)
  for name in ['','A'*64,'中'*22,'😀'*16,'18446744073709551615','a\0b','a\x7fb','a\nb']:
   self.assertFalse(response(json.dumps({'identity':'CanonicalUser','uid':None,'name':name}).encode())[0])
  for text in [b'\xc0\xaf',b'\xed\xa0\x80',b'\xf4\x90\x80\x80',b'\xe4\xb8',b'\xff',b'\x80']:
   body=b'{"identity":"CanonicalUser","uid":null,"name":"'+text+b'"}'
   self.assertFalse(response(body)[0]);self.assertFalse(lib.Name(text))
  for escape in [b'\\ud800',b'\\udc00',b'\\ud800\\u0041',b'\\u0000']:
   self.assertFalse(response(b'{"identity":"CanonicalUser","uid":null,"name":"'+escape+b'"}')[0])
 def test_every_truncation_and_malformed_metadata_is_rejected(self):
  body=b'{"identity":"CanonicalUser","uid":null,"name":"\\u4e2d\\u6587","metadata":[1,{"nested":true}]}'
  self.assertTrue(response(body)[0])
  for length in range(len(body)):
   self.assertFalse(response(body[:length])[0],length);COUNTS['truncations']+=1
  for bad in [body+b'trailing',b'[]',body[:-1]+b',}',body.replace(b'true',b'truex'),body.replace(b'[1,',b'[01,'),b'x'*4097]:
   self.assertFalse(response(bad)[0])
  nested=b'['*32+b'0'+b']'*32
  self.assertFalse(response(b'{"identity":"CanonicalUser","uid":null,"name":"Alias","ignored":'+nested+b'}')[0])
 def test_duplicate_identity_uid_or_name_is_rejected(self):
  for duplicate in [b'"identity":"Other",',b'"uid":"7",',b'"name":"Other",']:
   body=b'{'+duplicate+b'"identity":"CanonicalUser","uid":null,"name":"Alias"}'
   self.assertFalse(response(body)[0])
 def test_canonical_uid_session_generation_and_stale_worker_reply(self):
  lib.Reset();lib.Identity(b'CanonicalUser',7,0,100);initial=lib.Generation();lib.Snapshot()
  self.assertTrue(lib.Reply('中文'.encode(),1,100));self.assertEqual(alias(),(True,'中文'.encode()))
  self.assertFalse(alias(b'Other',7,100)[0]);self.assertFalse(alias(uid=8)[0])
  lib.Identity(b'CanonicalUser',7,0,200);self.assertEqual(lib.Generation(),initial)
  lib.Identity(b'CanonicalUser',7,1,201);self.assertGreater(lib.Generation(),initial)
  self.assertFalse(lib.Reply(b'Old session reply',1,202));self.assertFalse(alias(now=202)[0])
  lib.Snapshot();self.assertTrue(lib.Reply(b'Fresh',1,203));self.assertEqual(alias(now=203),(True,b'Fresh'))
  lib.Identity(b'Other',8,1,204);self.assertFalse(lib.Reply(b'Previous identity',1,205));self.assertFalse(alias(b'Other',8,205)[0])
 def test_exact_ttl_boundaries_wrap_and_failed_reply_fallback(self):
  for born in [0,1,0x7fffffff,0xfffffff0,0xffffffff]:
   lib.Reset();lib.Identity(b'CanonicalUser',7,0,born);lib.Snapshot();self.assertTrue(lib.Reply(b'Alias',1,born))
   for elapsed in [0,1,59999,60000,60001]:self.assertEqual(alias(now=(born+elapsed)&0xffffffff)[0],elapsed<60000)
   self.assertFalse(lib.Reply(b'Ignored',0,(born+1)&0xffffffff));self.assertFalse(alias(now=(born+2)&0xffffffff)[0])

if __name__=='__main__':
 start=time.perf_counter();result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Policy))
 report={'passed':result.wasSuccessful(),'methods':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
         'counts':COUNTS,'seconds':time.perf_counter()-start,'library_sha256':hashlib.sha256(args.library.read_bytes()).hexdigest(),
         'source_sha256':hashlib.sha256((Path(__file__).resolve().parents[1]/'kernel_name_policy.h').read_bytes()).hexdigest(),
         'boundary':'Compiled portable C JSON/UTF8/cache policy, independent Python oracle. No HTTP worker, native thunk, renderer or game acceptance.'}
 args.output.write_text(json.dumps(report,indent=2)+'\n');raise SystemExit(not result.wasSuccessful())
