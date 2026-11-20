import importlib.util,unittest,tempfile,zipfile
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tools/script_profile.py'
s=importlib.util.spec_from_file_location('profile',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class Profile(unittest.TestCase):
    def test_comments_strings_and_gameplay_notifications_preserved(self):
        text='f(){ iprintln("println(1); /# string #/"); /* println(2); */ // /#\n print("critical"); logprint("event"); }'
        self.assertEqual(m.quiet(text),(text,[]))
    def test_nested_calls_removed_without_evaluating_debug_arguments(self):
        text='f(){ if (ready) println("x" + gettime()); else act(); self iprintln("HUD"); }'
        result,edits=m.quiet(text)
        self.assertEqual(result,'f(){ if (ready) {} else act(); self iprintln("HUD"); }')
        self.assertEqual(len(edits),1)
    def test_declarations_namespaces_and_value_expressions_untouched(self):
        for text in ('println(x){return x;}','f(){ other::println(1); }','f(){ x=println(1); }'):
            self.assertEqual(m.quiet(text),(text,[]))
    def test_developer_block_and_line_numbers(self):
        text='f(){ /#\r\n developer();\r\n #/ live(); }'
        result,edits=m.quiet(text)
        self.assertNotIn('developer()',result);self.assertIn('live();',result)
        self.assertEqual(result.count('\n'),text.count('\n'));self.assertEqual(len(edits),1)
    def test_malformed_developer_blocks_fail(self):
        for text in ('/# f();','f(); #/','/# /# f(); #/ #/'):
            with self.assertRaises(ValueError):m.quiet(text)
    def test_idempotent_production_transform(self):
        result,_=m.quiet('f(){assert(x); print3d(a,b,c); line(a,b,c); /# debug(); #/ }')
        self.assertEqual(m.quiet(result),(result,[]))
        self.assertIn('assert(x);',result,'validation must not be removed as a performance shortcut')
    def test_archive_profiles_preserve_navigation_and_development_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'input.iwd'
            script=b'f(){ println("trace"); iprintln("HUD"); /# debug(); #/ }'
            navigation=b'node one\r\nnode two\r\n'
            with zipfile.ZipFile(source,'w') as z:
                z.writestr('scripts/maps/test.gsc',script);z.writestr('maps/test.csv',navigation)
            original=source.read_bytes()
            (root/'scr_vm_main.c').write_text('\n'.join('Scr_AddFunction("'+name+'", NativeFunction, 1);' for name in m.DEVELOPER_BUILTINS))
            for profile in ('production','development'):
                receipt=m.build(source,root/profile,profile,root)
                with zipfile.ZipFile(root/profile/'sh1x_svr_scripts.iwd') as z:
                    self.assertEqual(z.read('maps/test.csv'),navigation)
                    self.assertEqual(z.namelist(),['scripts/maps/test.gsc','maps/test.csv'])
                    self.assertEqual(z.read('scripts/maps/test.gsc'),script if profile=='development' else m.quiet(script.decode())[0].encode())
                self.assertEqual(source.read_bytes(),original)
                self.assertEqual(receipt['changed_scripts'],0 if profile=='development' else 1)
    def test_requires_exact_developer_registration(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);registration=root/'scr_vm_main.c'
            text='\n'.join('Scr_AddFunction("'+name+'", NativeFunction, 1);' for name in m.DEVELOPER_BUILTINS)
            registration.write_text(text)
            self.assertEqual(set(m.verify_developer_builtins(root)['registrations']),m.DEVELOPER_BUILTINS)
            for invalid in (text.replace(', 1);',', 0);',1),text+'\nScr_AddFunction("println", Other, 1);','// '+text.replace('\n','\n// ')):
                registration.write_text(invalid)
                with self.assertRaises(ValueError):m.verify_developer_builtins(root)
            with self.assertRaises(ValueError):m.build(root/'absent.iwd',root/'out','production')
            self.assertFalse((root/'out').exists())
    def test_production_verification_rejects_tracing_and_keeps_gameplay_assertions(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'scripts.iwd'
            for script,valid in ((b'f(){assert(ok);iprintln("HUD");logprint("event");}',True),
                                 (b'f(){println(side_effect());}',False),
                                 (b'f(){/# debug(); #/}',False)):
                with zipfile.ZipFile(source,'w') as z:z.writestr('scripts/maps/test.gsc',script)
                before=source.read_bytes()
                if valid:self.assertEqual(m.verify_archive(source)['scripts'],1)
                else:
                    with self.assertRaisesRegex(ValueError,'tracing remains'):m.verify_archive(source)
                self.assertEqual(source.read_bytes(),before)
if __name__=='__main__':unittest.main()
