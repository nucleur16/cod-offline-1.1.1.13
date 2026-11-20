"""Git-object provenance checks; no game processes or live files are used."""
from pathlib import Path
import hashlib,json,subprocess,tempfile,unittest,zipfile
from unittest.mock import patch
import verify_scripts as verifier

class ScriptProvenanceTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.native=self.root/'native';self.raw=self.root/'raw'
        for repo in (self.native,self.raw):
            repo.mkdir();subprocess.run(['git','init','-q',str(repo)],check=True)
            subprocess.run(['git','config','core.autocrlf','true'],cwd=repo,check=True)
        for repo,name,data in ((self.raw,'maps/test.gsc',b'qualified script\n'),
            (self.raw,'COLLATION_MANIFEST.json',b'bookkeeping'),
            (self.native,'bin/home/main/waypoints/test.csv',b'qualified route\n'),
            (self.native,'bin/home/main/mp/bot.csv',b'qualified bot\n')):
            target=repo/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        for repo in (self.native,self.raw):
            subprocess.run(['git','-c','core.autocrlf=false','add','.'],cwd=repo,check=True)
            subprocess.run(['git','-c','user.name=Fixture','-c','user.email=fixture@example.invalid',
                '-c','commit.gpgsign=false','commit','-qm','fixture'],cwd=repo,check=True)
        raw_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=self.raw,text=True).strip()
        self.files={'scripts/maps/test.gsc':b'qualified script\n','waypoints/test.csv':b'qualified route\n','mp/bot.csv':b'qualified bot\n'}
        self.pack=self.root/'scripts.iwd';self.manifest={'profiles':{'normal':{'raw_commit':raw_commit}}}
        (self.native/'native_profiles').mkdir();self.write_pack(self.files)

    def write_pack(self,files):
        with zipfile.ZipFile(self.pack,'w') as archive:
            for name,data in files.items():archive.writestr(name,data)
        self.manifest['profiles']['normal']['scripts_sha256']=hashlib.sha256(self.pack.read_bytes()).hexdigest()
        (self.native/'native_profiles/manifest.json').write_text(json.dumps(self.manifest),encoding='utf-8')

    def test_working_tree_edits_cannot_change_the_pinned_source(self):
        (self.raw/'maps/test.gsc').write_bytes(b'unqualified future script')
        (self.native/'bin/home/main/waypoints/test.csv').write_bytes(b'unqualified route')
        result=verifier.verify(self.native,self.raw,'normal',self.pack)
        self.assertTrue(result['passed']);self.assertEqual(result['files'],3)

    def test_retagged_archive_still_must_match_actual_git_blobs(self):
        changed=dict(self.files);changed['scripts/maps/test.gsc']=b'wrong script'
        self.write_pack(changed)
        with self.assertRaisesRegex(AssertionError,'pinned Git blob'):
            verifier.verify(self.native,self.raw,'normal',self.pack)

    def test_missing_and_extra_archive_entries_are_rejected(self):
        for change in ('missing','extra'):
            with self.subTest(change=change):
                files=dict(self.files)
                if change=='missing':files.pop('waypoints/test.csv')
                else:files['scripts/unreviewed.gsc']=b'extra'
                self.write_pack(files)
                with self.assertRaisesRegex(AssertionError,'inventory'):
                    verifier.verify(self.native,self.raw,'normal',self.pack)

    def test_wrong_artifact_is_rejected_before_git_objects_are_read(self):
        self.pack.write_bytes(self.pack.read_bytes()+b'drift')
        with patch.object(verifier,'expected_files',side_effect=AssertionError('must not read sources')) as read:
            with self.assertRaisesRegex(AssertionError,'qualified artifact'):
                verifier.verify(self.native,self.raw,'normal',self.pack)
        read.assert_not_called()

    def test_checkout_eol_requires_explicit_hash_bound_asset_scope(self):
        files=dict(self.files);files['mp/bot.csv']=b'qualified bot\r\n';self.write_pack(files)
        with self.assertRaisesRegex(AssertionError,'pinned Git blob'):
            verifier.verify(self.native,self.raw,'normal',self.pack)
        selected=self.manifest['profiles']['normal']
        selected['asset_checkout_crlf']={'mp/bot.csv':hashlib.sha256(b'qualified bot\n').hexdigest()}
        self.write_pack(files)
        self.assertTrue(verifier.verify(self.native,self.raw,'normal',self.pack)['passed'])
        selected['asset_checkout_crlf']['scripts/maps/test.gsc']=hashlib.sha256(self.files['scripts/maps/test.gsc']).hexdigest()
        self.write_pack(files)
        with self.assertRaisesRegex(AssertionError,'EOL scope'):
            verifier.verify(self.native,self.raw,'normal',self.pack)

if __name__=='__main__':unittest.main()
