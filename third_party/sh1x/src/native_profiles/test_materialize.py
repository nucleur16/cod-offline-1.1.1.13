"""Profile source preparation in ordinary and nested Git workspaces; no builds."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from materialize import materialize


def sha(data):
    return hashlib.sha256(data).hexdigest()


@contextmanager
def fixture(parent_git=False):
    with tempfile.TemporaryDirectory() as temporary:
        parent=Path(temporary)
        if parent_git: subprocess.run(['git','init','-q',str(parent)],check=True)
        repo=parent/'native';meta=repo/'native_profiles';meta.mkdir(parents=True)
        (repo/'src').mkdir()
        normal={'src/main.c':b'normal\n','src/obsolete.c':b'old\n'}
        zombies={'src/main.c':b'zombies\n','src/zombies.c':b'new\n'}
        for name,data in normal.items(): (repo/name).write_bytes(data)
        delta=b'''diff --git a/src/main.c b/src/main.c
--- a/src/main.c
+++ b/src/main.c
@@ -1 +1 @@
-normal
+zombies
diff --git a/src/obsolete.c b/src/obsolete.c
deleted file mode 100644
--- a/src/obsolete.c
+++ /dev/null
@@ -1 +0,0 @@
-old
diff --git a/src/zombies.c b/src/zombies.c
new file mode 100644
--- /dev/null
+++ b/src/zombies.c
@@ -0,0 +1 @@
+new
'''
        (meta/'zombies.patch').write_bytes(delta)
        for profile,files in (('normal',normal),('zombies',zombies)):
            (meta/(profile+'-code-files.json')).write_text(json.dumps({n:sha(v) for n,v in files.items()}))
        manifest={'routing':{'mp':'normal','coop':'normal','zm':'zombies'},'zombies_delta_sha256':sha(delta),
                  'native_libraries':{'test.a':sha(b'library')},'required_make_variables':{},
                  'profiles':{p:{'tested_native_sha256':None} for p in ('normal','zombies')}}
        (meta/'manifest.json').write_text(json.dumps(manifest))
        libraries=parent/'libraries';libraries.mkdir();(libraries/'test.a').write_bytes(b'library')
        yield parent,repo,libraries


class MaterializeTests(unittest.TestCase):
    def test_zombies_delta_applies_outside_and_inside_a_parent_git_workspace(self):
        for nested in (False,True):
            with self.subTest(nested=nested),fixture(nested) as (parent,repo,libraries):
                output=parent/'research'/'zombies'
                receipt=materialize(repo,output,'zm','zombies',libraries)
                self.assertEqual((output/'src/main.c').read_bytes(),b'zombies\n')
                self.assertFalse((output/'src/obsolete.c').exists())
                self.assertEqual((output/'src/zombies.c').read_bytes(),b'new\n')
                self.assertEqual((output/'lib-native/test.a').read_bytes(),b'library')
                self.assertEqual(receipt['source_files'],2)
                self.assertEqual((repo/'src/main.c').read_bytes(),b'normal\n')
                self.assertTrue((repo/'src/obsolete.c').exists())

    def test_inherited_git_directory_worktree_and_index_cannot_redirect_the_patch(self):
        with fixture(True) as (parent,repo,libraries):
            with patch.dict(os.environ,{'GIT_DIR':str(parent/'.git'),'GIT_WORK_TREE':str(parent),
                                         'GIT_INDEX_FILE':str(parent/'unrelated.index'),'GIT_PREFIX':'unrelated/'}):
                output=parent/'research'/'zombies'
                materialize(repo,output,'zm','zombies',libraries)
            self.assertEqual((output/'src/main.c').read_bytes(),b'zombies\n')
            self.assertFalse((parent/'unrelated.index').exists())

    def test_normal_profile_keeps_its_source_and_rejects_reuse_or_wrong_routing(self):
        with fixture(True) as (parent,repo,libraries):
            output=parent/'research'/'normal'
            materialize(repo,output,'mp','normal',libraries)
            self.assertEqual((output/'src/main.c').read_bytes(),b'normal\n')
            with self.assertRaisesRegex(AssertionError,'new'): materialize(repo,output,'mp','normal',libraries)
            with self.assertRaises(AssertionError): materialize(repo,parent/'wrong','coop','zombies',libraries)
            self.assertFalse((parent/'wrong').exists())

    def test_profile_make_variables_keep_shared_flags_and_select_exact_profile(self):
        with fixture() as (parent,repo,libraries):
            path=repo/'native_profiles/manifest.json'
            manifest=json.loads(path.read_text())
            manifest['required_make_variables']={'GAME':'codol','LOCAL_TEST':'0'}
            manifest['required_build_environment']={'SOURCE_DATE_EPOCH':'1792797849','TZ':'UTC','LC_ALL':'C'}
            for profile in ('normal','zombies'):
                manifest['profiles'][profile]['required_make_variables']={'BUILD_REVISION':profile+'-pinned'}
            path.write_text(json.dumps(manifest))
            for profile,category in [('normal','mp'),('zombies','zm')]:
                receipt=materialize(repo,parent/('qualified-'+profile),category,profile,libraries)
                self.assertEqual(receipt['required_make_variables'],{'GAME':'codol','LOCAL_TEST':'0','BUILD_REVISION':profile+'-pinned'})
                self.assertEqual(receipt['required_build_environment'],manifest['required_build_environment'])

    def test_resident_contract_is_distinct_from_source_manifest_and_checked_against_header(self):
        with fixture() as (parent,repo,libraries):
            header=repo/'src/codol/net/codol_persistent_build.h'
            header.parent.mkdir(parents=True)
            identity='a'*64
            header.write_bytes(('#define CODOL_RESIDENT_PROFILE_NAME "normal"\n#define CODOL_RESIDENT_PROFILE_SHA "'+identity+'"\n').encode())
            code=repo/'native_profiles/normal-code-files.json'
            names=json.loads(code.read_text());names[header.relative_to(repo).as_posix()]=sha(header.read_bytes());code.write_text(json.dumps(names))
            manifest_path=repo/'native_profiles/manifest.json';manifest=json.loads(manifest_path.read_text())
            manifest['profiles']['normal']['resident_contract_profile_sha256']=identity
            manifest_path.write_text(json.dumps(manifest))
            receipt=materialize(repo,parent/'correct','mp','normal',libraries)
            self.assertEqual(receipt['resident_contract_profile_sha256'],identity)
            self.assertNotEqual(receipt['source_manifest_sha256'],identity)
            manifest['profiles']['normal']['resident_contract_profile_sha256']='b'*64
            manifest_path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(AssertionError,'compiled contract identity'):
                materialize(repo,parent/'wrong-contract','mp','normal',libraries)

    def test_wrong_library_remains_rejected(self):
        with fixture(True) as (parent,repo,libraries):
            (libraries/'test.a').write_bytes(b'different library')
            with self.assertRaisesRegex(AssertionError,'wrong native library'):
                materialize(repo,parent/'research'/'normal','mp','normal',libraries)


if __name__=='__main__': unittest.main()
