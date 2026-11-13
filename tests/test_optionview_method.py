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
        selector=json.loads((ROOT/'tools/playlist-selector-producer/recipe.json').read_bytes())
        self.assertEqual(selector['prior_movie_sha256'],factor['output_movie_sha256'])
        self.assertEqual(selector['vertical_tag_sha256'],r['output_tag_sha256'])
        import importlib.util
        path=ROOT/'tools/p2p-four-tag-composition/composition.py'
        spec=importlib.util.spec_from_file_location('optionview_four_tag_composition',path)
        composition=importlib.util.module_from_spec(spec);spec.loader.exec_module(composition)
        proof=composition.verify(ROOT,selector['output_movie_sha256'])
        self.assertTrue(proof['inverse_exact']);self.assertEqual(proof['non_target_tags_exact'],6863)
        # The four-tag endpoint has a reviewed successor; the tree's movie is the end of that chain.
        self.assertEqual(proof['successor']['prior_movie_sha256'],proof['current_movie_sha256'])
        self.assertTrue(proof['successor']['inverse_exact'])
        self.assertIn('sha256 '+proof['successor']['current_movie_sha256'],lines)
        self.assertIn('as codo.views.OptionView as/codo/views/OptionView.as '+r['candidate_source_include_digest'],lines)
        self.assertFalse(any(D.is_relative_to(p)for p in [ROOT/'zones']))

class FourTagComposition(unittest.TestCase):
    def setUp(self):
        import importlib.util
        path=ROOT/'tools/p2p-four-tag-composition/composition.py'
        spec=importlib.util.spec_from_file_location('four_tag_composition_tests',path)
        self.C=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.C)
        self.contract=json.loads((path.parent/'composition.json').read_bytes())
        self.patch=ROOT/self.C.PATCH/'patch.txt'

    def changed_read(self,path,bad,reason):
        read=Path.read_bytes
        with patch.object(Path,'read_bytes',lambda p:bad if p==path else read(p)):
            with self.assertRaisesRegex(ValueError,reason):self.C.verify(ROOT,self.C.PRIOR_SHA)

    def test_actual_whole_movie_inverse_preserves_all_other6863_tags(self):
        p=self.C.verify(ROOT,self.C.PRIOR_SHA)
        self.assertEqual((p['qualified_classes'],p['non_target_tags_exact'],p['total_tags']),(4,6863,6867))
        self.assertTrue(p['inverse_exact']);self.assertFalse(p['runtime_or_release_ready'])

    def test_protected_OptionView_and_MatchController_mutants_refuse(self):
        for name in ('5988_82_codo_views_OptionView.bin','6167_82_codo_controllers_MatchController.bin'):
            path=ROOT/self.C.PATCH/'tags'/name;raw=path.read_bytes()
            with self.subTest(name=name):self.changed_read(path,raw[:-1]+bytes([raw[-1]^1]),'non-target tag payload unchanged')

    def test_each_unqualified_four_tag_mutation_refuses(self):
        for row in self.contract['classes']:
            path=ROOT/self.C.PATCH/row['tag'];raw=path.read_bytes()
            with self.subTest(cls=row['class_name']):self.changed_read(path,raw[:-1]+bytes([raw[-1]^1]),'qualified target tag identity')

    def test_qualified_Source_drift_refuses(self):
        for row in self.contract['classes']:
            path=ROOT/self.C.PATCH/row['source']
            with self.subTest(cls=row['class_name']):self.changed_read(path,path.read_bytes()+b'\n','composition DATA identity')

    def test_original_keep_order_and_stock_binding_are_required(self):
        raw=self.patch.read_bytes()
        for a,b,why in ((b'keep 0\n',b'keep 1\n','operations/order'),(b'stock patch_ui_mp:',b'stock ui_mp:','stock reference')):
            with self.subTest(a=a):
                self.assertIn(a,raw);self.changed_read(self.patch,raw.replace(a,b,1),why)

    def test_foreign_current_digest_or_header_geometry_refuses(self):
        raw=self.patch.read_bytes()
        current=self.C.verify(ROOT,self.C.PRIOR_SHA)['successor']['current_movie_sha256'].encode()
        self.assertIn(b'sha256 '+current+b'\n',raw)
        changed=raw.replace(current,b'0'*64,1)
        self.changed_read(self.patch,changed,'predecessor/current movie identities')
        # The four-tag endpoint's own digest is no longer an acceptable tree digest either.
        self.changed_read(self.patch,raw.replace(current,self.C.OUTPUT_SHA.encode(),1),'predecessor/current movie identities')
        line=next(x for x in raw.splitlines()if x.startswith(b'header '))
        changed=raw.replace(line,line[:-2]+(b'01'if line[-2:]!=b'01'else b'02'),1)
        self.changed_read(self.patch,changed,'header/version/geometry/fps')

    def test_extra_metadata_and_non_target_Source_repins_refuse(self):
        raw=self.patch.read_bytes()
        self.changed_read(self.patch,raw+b'format unrestricted\n','closed unique patch metadata')
        line=next(x for x in raw.splitlines()if x.startswith(b'as codo.views.OptionView '))
        self.changed_read(self.patch,raw.replace(line,line[:-64]+b'0'*64,1),'non-target Source metadata unchanged')

    def test_original_predecessor_identity_cannot_be_relabelled(self):
        with self.assertRaisesRegex(ValueError,'immutable selector predecessor'):self.C.verify(ROOT,self.C.OUTPUT_SHA)
        path=self.C.HERE/'selector-predecessor.gfx.zlib.DATA';raw=path.read_bytes()
        self.changed_read(path,raw[:-1]+bytes([raw[-1]^1]),'composition DATA identity')

    def test_duplicate_Source_and_trailing_metadata_bytes_refuse(self):
        raw=self.patch.read_bytes();line=next(x for x in raw.splitlines()if x.startswith(b'as '))
        self.changed_read(self.patch,raw+line+b'\n','unique canonical Source row')
        self.changed_read(self.patch,raw+b'# unqualified metadata drift\n','exact qualified patch metadata bytes')

class ThreeTagSuccessor(unittest.TestCase):
    """tools/p2p-deferred-host-composition: the deferred-host recompilation of three of the four P2P classes."""
    def setUp(self):
        import importlib.util
        def load(name,path):
            spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
        self.F=load('four_tag_predecessor_tests',ROOT/'tools/p2p-four-tag-composition/composition.py')
        self.S=load('three_tag_successor_tests',ROOT/'tools/p2p-deferred-host-composition/composition.py')
        self.X=load('resolved_abc_tests',self.S.HERE/'resolved_abc.py')
        self.contract=json.loads((self.S.HERE/'composition.json').read_bytes())
        self.review=json.loads((self.S.HERE/'automated-diff-review.json.DATA').read_bytes())

    def changed_read(self,path,bad,reason):
        read=Path.read_bytes
        with patch.object(Path,'read_bytes',lambda p:bad if p==path else read(p)):
            with self.assertRaisesRegex(ValueError,reason):self.F.verify(ROOT,self.F.PRIOR_SHA)

    def test_standalone_and_chained_proofs_agree_on_the_tree_movie(self):
        p=self.S.verify(ROOT,self.S.PRIOR_SHA)
        self.assertEqual(p,self.F.verify(ROOT,self.F.PRIOR_SHA)['successor'])
        self.assertEqual((p['qualified_classes'],p['non_target_tags_exact'],p['total_tags']),(3,6864,6867))
        self.assertEqual((p['prior_movie_sha256'],p['current_movie_sha256']),(self.F.OUTPUT_SHA,self.S.OUTPUT_SHA))
        self.assertTrue(p['inverse_exact']);self.assertFalse(p['runtime_or_release_ready']);self.assertFalse(p['human_reviewed'])
        self.assertIn('sha256 '+p['current_movie_sha256'],(ROOT/self.F.PATCH/'patch.txt').read_text(encoding='utf-8').splitlines())

    def test_predecessor_cannot_be_relabelled_or_supplied(self):
        with self.assertRaisesRegex(ValueError,'immutable four-tag predecessor'):self.S.verify(ROOT,self.S.OUTPUT_SHA)
        import types
        with self.assertRaisesRegex(ValueError,'reconstructed exact four-tag predecessor'):
            self.S.verify(ROOT,self.S.PRIOR_SHA,before=b'GFX',prior=types.SimpleNamespace(**vars(self.F)))

    def test_four_tag_stage_is_not_repinned(self):
        # The predecessor's contract and review record are the ones its own code has always pinned.
        self.assertEqual(self.F.CONTRACT_SHA,'6838fca3f273cf05b1e79a1ee3a85c944a83a177bcf00511a3686e194348ce8b')
        self.assertEqual(self.F.OUTPUT_SHA,self.S.PRIOR_SHA)
        self.assertEqual(self.review['predecessor_review']['sha256'],json.loads((self.F.HERE/'composition.json').read_bytes())['actual_semantic_peer_sha256'])

    def test_every_preserved_witness_and_stage_file_is_pinned(self):
        flip=lambda raw:raw[:-1]+bytes([raw[-1]^1])
        for row in self.contract['classes']:
            with self.subTest(cls=row['class_name']):
                path=self.S.HERE/row['prior_tag_data'];self.changed_read(path,flip(path.read_bytes()),'qualified target tag identity')
                path=self.S.HERE/row['prior_source_data'];self.changed_read(path,path.read_bytes()+b'\n','composition DATA identity')
        path=self.S.HERE/'patch.before.txt.DATA';self.changed_read(path,path.read_bytes()+b'\n','exact qualified patch metadata bytes')
        for name in ('composition.py','composition.json','automated-diff-review.json.DATA','resolved_abc.py'):
            with self.subTest(name=name):
                path=self.S.HERE/name;self.changed_read(path,path.read_bytes()+b'\n','composition DATA identity')

    def test_record_is_an_automated_review_and_says_so(self):
        r=self.review
        self.assertEqual((r['reviewer']['kind'],r['reviewer']['human'],r['runtime_authority'],r['actual_actions_authorized']),('automated',False,False,False))
        self.assertIn('No human reviewed',r['reviewer']['statement'])

    def test_resolved_delta_is_recomputed_from_the_actual_tags(self):
        edited={'codo.controllers.LobbyController':({'onChangeReadyStateHandle','onStartMatchHandle','onCampaignHostReady'},'beginDeferredLocalHost'),
                'codo.k33.LocalMatchControls':({'onProtocolFrame'},'refreshStatus'),
                'codo.k33.LocalCreateRoomOption':(set(),'label/set')}
        for row in self.contract['classes']:
            name=row['class_name'].replace('.','/')
            old=self.S.abc((self.S.HERE/row['prior_tag_data']).read_bytes(),name)
            new=self.S.abc((ROOT/self.F.PATCH/row['tag']).read_bytes(),name)
            d=self.X.delta(old,new);changed,added=edited[row['class_name']]
            with self.subTest(cls=row['class_name']):
                self.assertEqual(d,next(x for x in self.review['classes']if x['class_name']==row['class_name'])['resolved_delta'])
                self.assertEqual({m.rsplit('/',1)[1]for m in d['methods_changed_beyond_compiler_spelling']},changed)
                self.assertEqual(len(d['methods_added']),1);self.assertTrue(d['methods_added'][0].endswith('/'+added))
                self.assertEqual((d['methods_removed'],d['traits_removed'],len(d['traits_added'])),([],[],1))
                self.assertEqual(d['bodies_after'],d['bodies_before']+1)
                same=self.X.delta(old,old)
                self.assertEqual((same['methods_changed'],same['methods_added'],same['traits_added']),([],[],[]))
                # A one-byte operand change in a method body is a reported change, not noise.
                at=new.rfind(b'\xd0\x30');self.assertGreater(at,0) # getlocal0; pushscope -> getlocal1; pushscope
                try:mutated=self.X.delta(old,new[:at]+b'\xd1'+new[at+1:])
                except ValueError:mutated=None
                self.assertNotEqual(mutated,d)


if __name__=='__main__':unittest.main()
