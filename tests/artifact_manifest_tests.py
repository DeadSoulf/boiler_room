"""Exercise artifact notices and recursive hashes in an isolated local repository."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

PACKAGE = Path(__file__).resolve().parents[1]


class ArtifactManifestTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / 'ci').mkdir()
        shutil.copy2(PACKAGE / 'ci/manifest.py', self.root / 'ci/manifest.py')
        shutil.copytree(PACKAGE / 'licenses', self.root / 'licenses')
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Fixture',
                        '-c', 'user.email=fixture@example.invalid', 'commit', '-q',
                        '--allow-empty', '-m', 'Isolated artifact fixture'], check=True)

    def tearDown(self):
        self.temporary.cleanup()

    def manifest(self):
        subprocess.run([sys.executable, '-B', str(self.root / 'ci/manifest.py')], check=True)
        return json.loads((self.root / 'artifacts/output-sha256.json').read_text())

    def test_existing_notices_are_packaged_and_hashed(self):
        output = self.manifest()
        inputs = json.loads((self.root / 'artifacts/input-manifest.json').read_text())['sha256']
        for name in ('NotoSansDisplay-OFL.txt', 'guition-jc4880p4-bsp-LICENSE.txt', 'FONT_PROVENANCE.md'):
            original = (self.root / 'licenses' / name).read_bytes()
            self.assertEqual((self.root / 'artifacts/licenses' / name).read_bytes(), original)
            digest = hashlib.sha256(original).hexdigest()
            self.assertEqual(output['licenses/' + name], digest)
            self.assertEqual(inputs['licenses/' + name], digest)
        self.assertNotIn('output-sha256.json', output)

    def test_nested_same_names_and_updated_files_have_distinct_hashes(self):
        for name, content in [('first/log.txt', b'first'), ('second/log.txt', b'second')]:
            path = self.root / 'artifacts' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        output = self.manifest()
        self.assertEqual(output['first/log.txt'], hashlib.sha256(b'first').hexdigest())
        self.assertEqual(output['second/log.txt'], hashlib.sha256(b'second').hexdigest())
        (self.root / 'artifacts/second/log.txt').write_bytes(b'updated')
        output = self.manifest()
        self.assertEqual(output['second/log.txt'], hashlib.sha256(b'updated').hexdigest())
        self.assertNotIn('output-sha256.json', output)


if __name__ == '__main__':
    unittest.main(verbosity=2)
