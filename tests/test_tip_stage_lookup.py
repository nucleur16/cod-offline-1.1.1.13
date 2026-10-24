"""Original metadata layout plus actual AS Stage-lookup behavior; no native claim."""
from pathlib import Path
import hashlib,importlib.util,json,re,shutil,struct,subprocess,unittest
ROOT=Path(__file__).resolve().parents[1]
PATCH=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
DOC=ROOT/'docs/baselines/20261024-tip-stage-lookup'
spec=importlib.util.spec_from_file_location('tip_abc_audit',DOC/'abc_audit.py');ABC=importlib.util.module_from_spec(spec);spec.loader.exec_module(ABC)
def method(source,name):
    start=source.index('function '+name+'(');begin=source.index('{',start);end=begin+1;depth=1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    body=source[begin+1:end-1];body=re.sub(r'var (\w+):(Object|Boolean|String|Array|int|uint|Number|Point)',r'var \1',body)
    return re.sub(r'\(([^()]+) as DisplayObject\)',r'(\1)',body)
class TipStageLookup(unittest.TestCase):
    def test_original_class_metadata_and_method_inventory(self):
        tag=(PATCH/'tags/k33_82_codo_views_TipView.bin').read_bytes();header=6 if struct.unpack_from('<H',tag)[0]&63==63 else 2
        abc=ABC.read(tag[tag.index(b'\0',header+4)+1:])
        self.assertEqual(hashlib.sha256(abc['tables']).hexdigest(),'4a781aecb8559b54fd73a14dda64dccad112a78d701a1111df4423949b7a580a')
        self.assertEqual(len(abc['bodies']),12)
        self.assertEqual(abc['bodies'][7]['name'],'codo.views:TipView/private:onTipMouseMoveHandle')
    @unittest.skipUnless(shutil.which('node'),'Node is required for ordinary AS behavior fixture')
    def test_actual_source_uses_static_stage_and_preserves_tooltip_behavior(self):
        source=(PATCH/'as/codo/views/TipView.as').read_text(encoding='utf-8')
        names=('displayTip','removeTip','onTipMouseMoveHandle','onTipMouseOverHandle','onTipMouseOutHandle','tryTipMouseFollow')
        script=(DOC/'behavior.js').read_text(encoding='utf-8').replace('METHODS',json.dumps({n:method(source,n) for n in names}))
        run=subprocess.run([shutil.which('node'),'-e',script],capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(run.returncode,0,run.stderr);self.assertEqual(json.loads(run.stdout)['checks'],218)
if __name__=='__main__':unittest.main()
