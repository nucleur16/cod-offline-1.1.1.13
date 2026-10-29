"""Quit navigation behavior from actual AS bodies, plus compiled ABI preservation."""
from pathlib import Path
import importlib.util,json,re,shutil,struct,subprocess,unittest,zlib
ROOT=Path(__file__).resolve().parents[1]
PATCH=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
DOC=ROOT/'docs/baselines/20261029-quit-confirmation'
spec=importlib.util.spec_from_file_location('quit_abc',ROOT/'docs/baselines/20261024-tip-stage-lookup/abc_audit.py')
ABC=importlib.util.module_from_spec(spec);spec.loader.exec_module(ABC)
def body(source,name):
 start=source.index('function '+name+'(');start=source.index('{',start);end=start+1;depth=1
 while depth: depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return re.sub(r'var (\w+):\w+',r'var \1',source[start+1:end-1])
class QuitConfirmation(unittest.TestCase):
 def test_compiled_change_preserves_other_bodies_and_class_metadata(self):
  tag=(PATCH/'tags/6160_82_codo_controllers_PageNavigationController.bin').read_bytes()
  h=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
  before=zlib.decompress((DOC/'PageNavigationController.before.abc.zlib').read_bytes())
  after=tag[tag.index(b'\0',h+4)+1:]
  self.assertEqual(ABC.compare(before,after,['gotoPageByPageObj']),json.loads((DOC/'review.json').read_text()))
 def execute(self,navigation):
  main=(PATCH/'as/codo/controllers/MainMenuController.as').read_text()
  summary=(PATCH/'as/codo/views/SummaryView.as').read_text()
  methods={'navigate':body(navigation,'gotoPageByPageObj'),'request':body(main,'onGotoQuitHandle'),
           'back':body(summary,'onConfirmQuitGameHandle'),'confirm':body(summary,'onCancelQuitGameHandle')}
  js=(DOC/'behavior.js').read_text().replace('METHODS',json.dumps(methods))
  return subprocess.run([shutil.which('node'),'-e',js],capture_output=True,text=True)
 def test_quit_opens_progress_before_confirm_and_back_does_not_exit(self):
  self.assertIsNotNone(shutil.which('node'))
  result=self.execute((PATCH/'as/codo/controllers/PageNavigationController.as').read_text())
  self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(json.loads(result.stdout)['scenarios'],4)
 def test_original_first_login_bypass_is_detected(self):
  result=self.execute(zlib.decompress((DOC/'PageNavigationController.before.as.zlib').read_bytes()).decode())
  self.assertNotEqual(result.returncode,0)
  self.assertIn('quit request bypassed confirmation',result.stderr)
if __name__=='__main__':unittest.main()
