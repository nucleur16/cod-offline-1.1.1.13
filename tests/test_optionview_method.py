"""Portable real ABC/source DATA and pre-IO build-route regression contracts."""
from pathlib import Path
import ast,hashlib,io,json,os,sys,types,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
D=ROOT/'tools/optionview-method-producer'
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(D))
import reproduce as R,swf_data as S,verify as V,transplant as T
from zonekit import gfx

class FirstIO(Exception):pass
def stop(*a,**kw):raise FirstIO('original IO reached')
def function(path,name):
    return next(n for n in ast.parse(path.read_text(encoding='utf-8')).body if isinstance(n,ast.FunctionDef)and n.name==name)

class MethodSource(unittest.TestCase):
    def test_actual_vertical_centers_and_budget_are_verified(self):
        output,proof=R.reproduce()
        semantic=proof['checks']['semantic']
        self.assertTrue(semantic['both_centers_before_single_setter'])
        self.assertTrue(semantic['both_centers_restored'])
        self.assertTrue(semantic['locals4_to6'])

    def test_actual_height_reference_cannot_be_substituted_with_width(self):
        original=(D/'original.tag.bin').read_bytes()
        current=(R.PATCH/'tags/5988_82_codo_views_OptionView.bin').read_bytes()
        raw=S.abc(current);parsed=V.unique39(raw);target,_=T.target(parsed)
        row=next(z for z in V.L.parse(raw)['bodies']if z['method']==target)
        # Actual target's two height reads must remain distinct from width.
        needle=b'\x66'+V.L.u30(170);replacement=b'\x66'+V.L.u30(169)
        self.assertEqual(row['code'].count(needle),2)
        at=row['begin']+raw[row['begin']:row['end']].index(row['code'])
        changed=raw[:at]+row['code'].replace(needle,replacement)+raw[at+len(row['code']):]
        bad=current[:-len(raw)]+changed
        with self.assertRaisesRegex(ValueError,'saved x/y'):
            V.compiled(original,bad,R.expanded_source())

    def test_export_without_y_restore_is_refused(self):
        source=R.expanded_source()
        self.assertEqual(source.count('tf.y = h - tf.height / 2;'),1)
        with self.assertRaises(ValueError):
            V.exported(source.replace('tf.y = h - tf.height / 2;','',1))

    def test_actual_tag_reproduces_only_target_and_preserves_original38(self):
        output,proof=R.reproduce()
        self.assertTrue(proof['original_pools_tables_and_other38_byteexact'])
        self.assertTrue(proof['candidate_other38_NOT_PUBLISHED'])
        before=(D/'base.abc').read_bytes();after=S.abc(output)
        a,b=V.unique39(before),V.unique39(after);target,_=T.target(a)
        self.assertEqual(a['tables'],b['tables']);self.assertEqual(a['pools'],b['pools'])
        changed=[i for i in a['bodies']if a['bodies'][i]['raw']!=b['bodies'][i]['raw']]
        self.assertEqual(changed,[target]);self.assertEqual(len(a['bodies']),39)

    def test_duplicate_method_inventory_refuses_without_hash_shortcut(self):
        raw=(D/'base.abc').read_bytes();parsed=V.L.parse(raw);a,b=parsed['bodies'][:2]
        # Replace just the second method ID with the first: still a declared
        # 39-row inventory, no longer 39 unique IDs. No recipe/hash gate used.
        reader=V.A.Reader(raw[b['begin']:]);reader.u30()
        changed=raw[:b['begin']]+V.L.u30(a['method'])+raw[b['begin']+reader.pos:]
        with self.assertRaisesRegex(ValueError,'39 unique'):V.unique39(changed)

    def test_original_uncentered_target_refuses_semantics(self):
        original=(D/'original.tag.bin').read_bytes()
        with self.assertRaisesRegex(ValueError,'local/stack'):V.center_semantics(S.abc(original),S.abc(original),R.expanded_source())

    def test_changed_other_body_refuses_without_output_hash_gate(self):
        original=(D/'original.tag.bin').read_bytes();candidate=(R.PATCH/'tags/5988_82_codo_views_OptionView.bin').read_bytes()
        raw=S.abc(candidate);a=V.unique39(raw);target,_=T.target(a)
        row=next(z for z in V.L.parse(raw)['bodies']if z['method']!=target)
        # Change a retained raw body byte inside its method ID/local budget.
        offset=row['begin']+len(V.L.u30(row['method']))
        changed=bytearray(raw);changed[offset]^=1
        head=candidate[:-len(raw)];bad=bytearray(head+changed)
        with self.assertRaises(ValueError):V.compiled(original,bytes(bad),R.expanded_source())

    def test_source_include_drift_is_not_auto_recompiled(self):
        source=R.PATCH/'as/codo/views/OptionView.as';read=Path.read_bytes
        with patch.object(Path,'read_bytes',lambda p:read(p)+b'\n'if p==source else read(p)):
            with self.assertRaisesRegex(ValueError,'source identity'):R.expanded_source()

    def invoke(self,name,cls):
        fn=function(ROOT/'zonekit/gfx.py',name)
        env=dict(os=os,open=stop,tempfile=types.SimpleNamespace(TemporaryDirectory=stop))
        exec(compile(ast.Module(body=[fn],type_ignores=[]),'<actual-route>','exec'),env)
        return env[name](b'movie',cls,'source','ffdec')if name=='compile_as'else env[name]('owned',b'stock',cls,'source','ffdec')

    def test_generic_routes_normalized_target_refused_before_IO(self):
        for name in ('compile_as','store_class'):
            for cls in ('codo.views.OptionView','codo/views/OptionView','codo.views/OptionView','codo/views.OptionView'):
                with self.subTest(name=name,cls=cls),self.assertRaisesRegex(SystemExit,'method-only producer'):self.invoke(name,cls)

    def test_other_and_unknown_class_routes_preserve_original_first_IO(self):
        for name in ('compile_as','store_class'):
            for cls in ('codo.views.OtherView','codo/views/OtherView',' codo.views.OptionView','codo//views/OptionView','codo\\views\\OptionView','Codo.views.OptionView'):
                with self.subTest(name=name,cls=cls),self.assertRaises(FirstIO):self.invoke(name,cls)

    def build_route(self,cls,edited,all_sources):
        fn=function(ROOT/'zonekit/build.py','compile_as_files')
        class BuildError(Exception):pass
        stub=types.SimpleNamespace(read_patch=lambda p:({'as':[(cls,'as/codo/views/OptionView.as','expected')]},None),source_sha=lambda p:'changed'if edited else'expected',export_as=stop,compile_as=stop)
        env=dict(os=os,gfx=stub,BuildError=BuildError,tempfile=types.SimpleNamespace(TemporaryDirectory=stop))
        exec(compile(ast.Module(body=[fn],type_ignores=[]),'<actual-build-route>','exec'),env)
        return env['compile_as_files'](str(R.PATCH),b'stored',b'stock',__file__,all_sources,lambda s:None)

    def test_normal_build_uses_matching_stored_tag_without_export(self):
        self.assertEqual(self.build_route('codo.views.OptionView',False,False),b'stored')

    def test_edited_or_compile_all_target_stops_before_export_and_compile(self):
        for cls in ('codo.views.OptionView','codo/views/OptionView','codo.views/OptionView'):
            for edited,all_sources in ((True,False),(False,True),(True,True)):
                with self.subTest(cls=cls,edited=edited,all_sources=all_sources),self.assertRaisesRegex(Exception,'method-only producer'):self.build_route(cls,edited,all_sources)

    def test_unrelated_compile_all_preserves_export_route(self):
        with self.assertRaises(FirstIO):self.build_route('codo.views.OtherView',False,True)

    def test_actual_pins_and_movie_representations_are_separate(self):
        r=R.recipe();self.assertEqual(gfx.source_sha(str(R.PATCH/'as/codo/views/OptionView.as')),r['candidate_source_include_digest'])
        self.assertEqual(r['whole_class_baseline'],'HOLD');self.assertFalse(r['FF_rebuilt']);self.assertFalse(r['game_or_release_ready'])
        self.assertNotEqual(r['output_movie_sha256'],r['diagnostic_FWS_sha256'])
        lines=(R.PATCH/'patch.txt').read_text(encoding='utf-8').splitlines()
        # The factor DATA successor composes over this reviewed vertical movie;
        # keep this component's original digest rather than relabel its proof.
        factor=json.loads((ROOT/'tools/training-factor-producer/recipe.json').read_bytes())
        self.assertEqual(factor['vertical_component_movie_sha256'],r['output_movie_sha256'])
        self.assertEqual(factor['vertical_component_tag_sha256'],r['output_tag_sha256'])
        self.assertIn('sha256 '+factor['output_movie_sha256'],lines)
        self.assertIn('as codo.views.OptionView as/codo/views/OptionView.as '+r['candidate_source_include_digest'],lines)
        self.assertFalse(any(D.is_relative_to(p)for p in [ROOT/'zones']))

    def test_real_CI_outcome_gate_requires_original12_and_new_method_check(self):
        import contextlib
        workflow=(ROOT/'.github/workflows/shared-ui-baseline.yml').read_text(encoding='utf-8')
        code=workflow.split("          python - <<'PY'\n",1)[1].split('          PY',1)[0]
        code='\n'.join(line[10:]for line in code.splitlines())
        complete={'native_check_%02d'%i:{'outcome':'success'}for i in range(1,18)}
        for missing in (None,1,7,12,13,14,15,16,17):
            rows=dict(complete)
            if missing:rows.pop('native_check_%02d'%missing)
            with self.subTest(missing=missing),patch.dict(os.environ,NATIVE_QUALIFICATION_STEPS=json.dumps(rows)),contextlib.redirect_stdout(io.StringIO()):
                if missing:
                    with self.assertRaisesRegex(SystemExit,'failed or did not run'):exec(compile(code,'<real-CI-outcome-gate>','exec'),{})
                else:exec(compile(code,'<real-CI-outcome-gate>','exec'),{})
        for check in ('native_check_13','native_check_14','native_check_15','native_check_16','native_check_17'):
            for outcome in ('failure','skipped','cancelled'):
                rows=dict(complete);rows[check]={'outcome':outcome}
                with patch.dict(os.environ,NATIVE_QUALIFICATION_STEPS=json.dumps(rows)),contextlib.redirect_stdout(io.StringIO()),self.assertRaises(SystemExit):exec(compile(code,'<real-CI-outcome-gate>','exec'),{})

if __name__=='__main__':unittest.main()
