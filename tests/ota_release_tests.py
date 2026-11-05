"""Fault checks for the exact assets used by USB and GitHub OTA.

Synthetic images/partition tables only; never accesses a device or the network.
"""
from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import shutil
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from io import StringIO
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ci'))
import ota_manifest as ota


class PublisherBodyTests(unittest.TestCase):
    def test_json_manifest_is_uploaded_as_exact_bytes(self):
        spec = importlib.util.spec_from_file_location('checked_publisher', ROOT / 'scripts/publish_github_release.py')
        publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(publisher)
        manifest = b'{"schema": 1, "version": "v0.10.0"}\n'
        self.assertIs(publisher.request_body(manifest, upload=True), manifest)
        for content in (b'PK\x03\x04\x00\xff', b'0123456789  firmware.bin\n'):
            self.assertIs(publisher.request_body(content, upload=True), content)
        self.assertEqual(json.loads(publisher.request_body({'draft': True})), {'draft': True})
        self.assertIsNone(publisher.request_body(None))
        with self.assertRaises(TypeError):
            publisher.request_body({'schema': 1}, upload=True)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for folder in ('src', 'scripts', 'artifacts'):
            (self.root / folder).mkdir()
        shutil.copy2(ROOT / 'scripts/flash_panel.py', self.root / 'scripts/flash_panel.py')
        self.image = bytearray(65536)
        self.image[0] = 0xe9
        struct.pack_into('<H', self.image, 12, 18)
        struct.pack_into('<HH', self.image, 15, 0, 65535)
        self.image = bytes(self.image)
        self.boot = self.image[:256]
        (self.root / 'src/panel_update_build.h').write_text(
            f'#define BOILER_BOOTLOADER_SHA256 "{hashlib.sha256(self.boot).hexdigest()}"\n'
            f'#define BOILER_BOOTLOADER_BYTES {len(self.boot)}\n', encoding='ascii')
        self.checks = ota._flash_checks(self.root)
        records = b''.join(struct.pack('<HBBII16sI', 0x50aa, kind, subtype, offset,
                                     size, name.encode(), flags)
                           for kind, subtype, offset, size, name, flags
                           in self.checks.EXPECTED_PARTITIONS)
        self.table = records + b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(records).digest()
        self.tag = 'v0.10.0'
        self.commit = 'e' * 40

    def write(self, **change):
        args = dict(root=self.root, version=self.tag, commit=self.commit,
                    firmware=self.image, bootloader=self.boot, partitions=self.table)
        args.update(change)
        return ota.write_assets(**args)

    def validate(self):
        return ota.validate_assets(self.root, self.tag, self.commit, self.image, self.boot, self.table)

    def test_complete_assets_identify_the_exact_build(self):
        manifest = self.write()
        self.assertEqual(self.validate(), manifest)
        self.assertEqual(manifest['source_commit'], self.commit)
        self.assertEqual(manifest['firmware']['size'], len(self.image))
        self.assertEqual(manifest['firmware']['sha256'], hashlib.sha256(self.image).hexdigest())
        self.assertEqual(manifest['firmware']['url'],
                         'https://github.com/DeadSoulf/boiler_room/releases/download/'
                         'v0.10.0/BoilerPanel-v0.10.0-firmware.bin')
        self.assertEqual((self.root / 'artifacts' / manifest['firmware']['name']).read_bytes(), self.image)

    def test_bad_version_or_nonimmutable_source_is_refused(self):
        for version in ('v01.2.3', 'v1.2.3-rc1', '1.2.3', 'v1.2.3/x', 'v4294967296.0.0'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                self.write(version=version)
        for commit in ('main', 'e' * 7, 'E' * 40, 'g' * 40):
            with self.subTest(commit=commit), self.assertRaises(ValueError):
                self.write(commit=commit)

    def test_wrong_image_chip_revision_or_slot_size_is_refused(self):
        wrong_chip = bytearray(self.image); struct.pack_into('<H', wrong_chip, 12, 9)
        wrong_revision = bytearray(self.image); struct.pack_into('<HH', wrong_revision, 15, 300, 399)
        for image in (bytes(wrong_chip), bytes(wrong_revision), self.image[:200],
                      self.image + b'\0' * self.checks.APP_BYTES):
            with self.subTest(size=len(image)), self.assertRaises((ValueError, self.checks.Refusal, RuntimeError)):
                self.write(firmware=image)

    def test_unknown_bootloader_and_modified_partition_table_are_refused(self):
        with self.assertRaises(ValueError):
            self.write(bootloader=self.boot[:-1] + b'x')
        table = bytearray(self.table); table[5] ^= 1
        with self.assertRaises(RuntimeError):
            self.write(partitions=bytes(table))
        records = bytearray(self.table[:-32]); records[8] ^= 1
        wrong_layout = bytes(records) + b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(records).digest()
        with self.assertRaises(ValueError):
            self.write(partitions=wrong_layout)

    def test_stale_manifest_substituted_url_and_extra_fields_are_refused(self):
        original = self.write()
        variants = []
        for key, value in [('source_commit', 'a' * 40), ('version', 'v0.9.1'),
                           ('schema', True), ('flash_bytes', 16777216.0), ('extra', 'unknown')]:
            altered = copy.deepcopy(original); altered[key] = value; variants.append(altered)
        for key, value in [('url', 'https://example.invalid/firmware.bin'),
                           ('size', len(self.image) - 1), ('sha256', '0' * 64)]:
            altered = copy.deepcopy(original); altered['firmware'][key] = value; variants.append(altered)
        for value in variants:
            with self.subTest(value=value):
                (self.root / 'artifacts' / ota.MANIFEST_NAME).write_text(json.dumps(value))
                with self.assertRaises(ValueError):
                    self.validate()

    def test_duplicate_fields_bom_and_changed_raw_image_are_refused(self):
        self.write()
        path = self.root / 'artifacts' / ota.MANIFEST_NAME
        original = path.read_bytes()
        for data in (b'\xef\xbb\xbf' + original,
                     original.replace(b'"schema": 1,', b'"schema": 1, "schema": 1,')):
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                self.validate()
        manifest = self.write()
        (self.root / 'artifacts' / manifest['firmware']['name']).write_bytes(self.image[:-1] + b'x')
        with self.assertRaises(ValueError):
            self.validate()


class FakeGitHub:
    """Deterministic release API only; no credentials, sockets or real publication."""
    def __init__(self, release=None):
        self.release = copy.deepcopy(release)
        self.calls = []
        self.fail_upload = None

    def __call__(self, method, path, data=None, mime='application/json', upload=False, allow404=False):
        self.calls.append((method, path, copy.deepcopy(data), upload))
        if method == 'GET':
            if self.release is None:
                assert allow404
                return None
            return copy.deepcopy(self.release)
        if method == 'POST' and path == '/releases':
            assert self.release is None and data['draft'] is True
            self.release = dict(data, id=123, assets=[], html_url='https://github.com/DeadSoulf/boiler_room/releases/tag/' + data['tag_name'])
            return copy.deepcopy(self.release)
        if method == 'POST' and upload:
            assert type(data) is bytes and self.release['draft'] is True
            name = parse_qs(urlsplit(path).query)['name'][0]
            if name == self.fail_upload:
                raise RuntimeError('simulated upload failure')
            asset = {'name': name, 'size': len(data), 'digest': 'sha256:' + hashlib.sha256(data).hexdigest()}
            self.release['assets'].append(asset)
            return copy.deepcopy(asset)
        if method == 'PATCH':
            self.release.update(data)
            return copy.deepcopy(self.release)
        raise AssertionError((method, path))

    def mutations(self):
        return [call for call in self.calls if call[0] != 'GET']


class CandidatePublisherTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('candidate_publisher', ROOT / 'scripts/publish_github_release.py')
        self.publisher = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.publisher)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for folder in ('src', 'tests', 'artifacts/equipment-web-review'):
            (self.root / folder).mkdir(parents=True, exist_ok=True)
        self.tag, self.source = 'v0.10.1', 'a' * 40
        self.committed = {'src/panel_web_page.h': b'fixture embedded page\n',
                          'tests/equipment_web_browser.py': b'# fixture regression source\n'}
        for name, data in self.committed.items():
            (self.root / name).write_bytes(data)
        self.payloads = [(f'BoilerPanel-{self.tag}-COM3.zip', b'fixture ZIP bytes', 'application/zip'),
                         (f'BoilerPanel-{self.tag}-firmware.bin', b'fixture application bytes', 'application/octet-stream'),
                         (ota.MANIFEST_NAME, b'{"fixture":"exact manifest bytes"}\n', 'application/json')]
        sums = ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name, data, _ in self.payloads).encode('ascii')
        self.payloads.append(('SHA256SUMS.txt', sums, 'text/plain'))
        self.report = {'result': 'PASS', 'test_script': self.publisher.BROWSER_TEST,
                       'source_header': str(self.root / self.publisher.WEB_HEADER),
                       'source_header_sha256': hashlib.sha256(self.committed[self.publisher.WEB_HEADER]).hexdigest(),
                       'test_script_sha256': hashlib.sha256(self.committed[self.publisher.BROWSER_TEST]).hexdigest(),
                       'widths': [320, 390, 768, 1280], 'page_errors': [], 'mutation_requests': [],
                       'physical_panel_contacted': False, 'status_polls': 12, 'coverage': ['real HTTP polls retain expanded details']}
        self.proof = {'schema': 1, 'kind': 'ota-build-candidate', 'project': 'DeadSoulf/boiler_room',
                      'version': self.tag, 'source_commit': self.source, 'hardware_acceptance': 'pending',
                      'assets': self.publisher.artifact_bindings(self.payloads),
                      'checks': {'final_build': {'source_commit': self.source,
                                                'firmware_sha256': hashlib.sha256(self.payloads[1][1]).hexdigest(),
                                                'target': 'jc4880p4', 'exit_code': 0,
                                                'log': self.evidence('artifacts/final-build.log', b'Processing jc4880p4\n[SUCCESS] Took 30 seconds\n')},
                                 'browser_tests': {'source_commit': self.source, 'test_script': self.publisher.BROWSER_TEST,
                                                   'report': self.evidence('artifacts/equipment-web-review/browser-result.json',
                                                                           json.dumps(self.report).encode())}}}

    def evidence(self, name, data):
        (self.root / name).write_bytes(data)
        return {'path': name, 'sha256': hashlib.sha256(data).hexdigest()}

    def validate(self, proof=None, payloads=None, committed=None):
        return self.publisher.validate_candidate(self.root, self.proof if proof is None else proof,
                                                 self.tag, self.source, self.payloads if payloads is None else payloads,
                                                 source_file=(self.committed if committed is None else committed).__getitem__)

    def hardware(self, ota_accepted=False):
        proof = {'version': self.tag, 'source_commit': self.source,
                 'firmware_sha256': hashlib.sha256(self.payloads[1][1]).hexdigest(),
                 'readback_and_metadata_verified': True, 'http_ok': True}
        if ota_accepted:
            proof.update(zip_sha256=hashlib.sha256(self.payloads[0][1]).hexdigest(),
                         ota_manifest_sha256=hashlib.sha256(self.payloads[2][1]).hexdigest(),
                         network_ota_install_verified=True, ota_metadata_unchanged=False)
        return proof

    def publish(self, api, mode='candidate', notes_only=False):
        proof = self.proof if mode == 'candidate' else self.hardware(True)
        notes = self.publisher.release_notes('Сохранено состояние раскрытых карточек.', mode, self.tag, self.source, proof)
        return self.publisher.publish_release(api, self.tag, self.source, notes, self.payloads, mode, notes_only)

    def test_exact_candidate_accepts_build_and_browser_proof_without_hardware_claim(self):
        self.validate()
        notes = self.publisher.release_notes('Изменение интерфейса.', 'candidate', self.tag, self.source, self.proof)
        self.assertTrue(notes.startswith(self.publisher.CANDIDATE_MARKER))
        self.assertIn('аппаратная приёмка ожидается', notes)
        self.assertIn('ещё не подтверждена', notes)
        self.assertNotIn('Аппаратная установка подтверждена', notes)
        for marker in (self.publisher.HARDWARE_MARKER, self.publisher.CANDIDATE_MARKER):
            with self.assertRaises(ValueError):
                self.publisher.release_notes(marker, 'candidate', self.tag, self.source, self.proof)

    def test_missing_unknown_mixed_or_false_hardware_claims_are_refused(self):
        for key, value in [('schema', True), ('kind', 'hardware-accepted'), ('project', 'other/repo'),
                           ('version', 'v0.10.0'), ('source_commit', 'b' * 40),
                           ('hardware_acceptance', 'accepted'), ('hardware_verified', True)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                proof = copy.deepcopy(self.proof); proof[key] = value; self.validate(proof)
        for key in self.proof:
            with self.subTest(missing=key), self.assertRaises(ValueError):
                proof = copy.deepcopy(self.proof); del proof[key]; self.validate(proof)
        for key in ('final_build', 'browser_tests'):
            with self.subTest(missing_check=key), self.assertRaises(ValueError):
                proof = copy.deepcopy(self.proof); del proof['checks'][key]; self.validate(proof)

    def test_each_exact_asset_and_its_size_is_bound_to_the_proof(self):
        for index in range(3):
            with self.subTest(index=index), self.assertRaises(ValueError):
                changed = list(self.payloads); name, data, mime = changed[index]; changed[index] = (name, data + b'changed', mime)
                self.validate(payloads=changed)
            with self.subTest(float_size=index), self.assertRaises(ValueError):
                proof = copy.deepcopy(self.proof); name = self.payloads[index][0]
                proof['assets'][name]['size'] = float(proof['assets'][name]['size']); self.validate(proof)
        for change in ('missing', 'extra', 'digest'):
            proof = copy.deepcopy(self.proof)
            if change == 'missing': del proof['assets'][ota.MANIFEST_NAME]
            elif change == 'extra': proof['assets']['private-backup.bin'] = {'size': 1, 'sha256': '0' * 64}
            else: proof['assets'][ota.MANIFEST_NAME]['sha256'] = '0' * 64
            with self.subTest(change=change), self.assertRaises(ValueError): self.validate(proof)

    def test_final_build_must_pass_for_exact_source_and_firmware(self):
        for key, value in [('source_commit', 'b' * 40), ('firmware_sha256', '0' * 64),
                           ('target', 'another-board'), ('exit_code', 1), ('exit_code', False)]:
            proof = copy.deepcopy(self.proof); proof['checks']['final_build'][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): self.validate(proof)
        for log in (b'', b'compiler still running', b'[FAILED]', b'[SUCCESS]\n[FAILED]'):
            proof = copy.deepcopy(self.proof)
            proof['checks']['final_build']['log'] = self.evidence('artifacts/final-build.log', log)
            with self.subTest(log=log), self.assertRaises(ValueError): self.validate(proof)

    def test_evidence_paths_hashes_and_actual_committed_files_are_checked(self):
        for path in ('', '../outside.log', '/tmp/outside.log', 'artifacts/../outside.log',
                     'artifacts//final-build.log', 'C:\\private.log'):
            proof = copy.deepcopy(self.proof); proof['checks']['final_build']['log']['path'] = path
            with self.subTest(path=path), self.assertRaises(ValueError): self.validate(proof)
        proof = copy.deepcopy(self.proof); proof['checks']['browser_tests']['report']['sha256'] = '0' * 64
        with self.assertRaises(ValueError): self.validate(proof)
        proof = copy.deepcopy(self.proof); proof['checks']['final_build']['log']['path'] = 'artifacts/missing.log'
        with self.assertRaises(FileNotFoundError): self.validate(proof)
        for name in self.committed:
            committed = dict(self.committed); committed[name] += b'from another commit'
            with self.subTest(committed=name), self.assertRaises(ValueError): self.validate(committed=committed)
            original = (self.root / name).read_bytes(); (self.root / name).write_bytes(original + b'uncommitted')
            with self.subTest(worktree=name), self.assertRaises(ValueError): self.validate()
            (self.root / name).write_bytes(original)

    def test_baseline_failed_incomplete_or_unbound_browser_results_are_refused(self):
        for key, value in [('result', 'REPRODUCED'), ('result', 'FAIL'), ('page_errors', ['exception']),
                           ('mutation_requests', [{'method': 'POST'}]), ('physical_panel_contacted', True),
                           ('widths', [1280]), ('status_polls', 0), ('status_polls', True), ('coverage', []),
                           ('source_header_sha256', '0' * 64), ('test_script_sha256', '0' * 64),
                           ('test_script', 'tests/unrelated.py')]:
            report = copy.deepcopy(self.report); report[key] = value
            proof = copy.deepcopy(self.proof)
            proof['checks']['browser_tests']['report'] = self.evidence('artifacts/equipment-web-review/browser-result.json', json.dumps(report).encode())
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): self.validate(proof)

    def test_proof_json_rejects_duplicate_keys_bom_and_nan(self):
        for raw in (b'\xef\xbb\xbf{}', b'{"schema":1,"schema":1}', b'{"schema":NaN}', b'[]'):
            with self.subTest(raw=raw), self.assertRaises(ValueError): self.publisher.strict_json(raw, 'proof')

    def test_modes_are_required_and_mutually_exclusive(self):
        base = ['--source', self.source, '--package', 'artifacts/package.zip', '--notes', 'notes.md']
        parser = self.publisher.argument_parser()
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit): parser.parse_args(base)
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            parser.parse_args(base + ['--hardware-result', 'hw.json', '--ota-build-result', 'build.json'])
        self.assertIsNone(parser.parse_args(base + ['--hardware-result', 'hw.json']).ota_build_result)
        self.assertIsNone(parser.parse_args(base + ['--ota-build-result', 'build.json']).hardware_result)

    def test_existing_usb_hardware_mode_is_preserved_and_notes_only_needs_real_ota(self):
        legacy = self.hardware()
        self.publisher.validate_hardware(legacy, self.tag, self.source, self.payloads)
        self.publisher.validate_hardware(self.hardware(True), self.tag, self.source, self.payloads, notes_only=True)
        for key, value in [('version', 'v0.10.0'), ('source_commit', 'b' * 40), ('firmware_sha256', '0' * 64),
                           ('readback_and_metadata_verified', 1), ('http_ok', False),
                           ('kind', 'ota-build-candidate'), ('hardware_acceptance', 'pending')]:
            altered = dict(legacy); altered[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.publisher.validate_hardware(altered, self.tag, self.source, self.payloads)
        for key in ('zip_sha256', 'ota_manifest_sha256', 'network_ota_install_verified'):
            proof = self.hardware(True); del proof[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                self.publisher.validate_hardware(proof, self.tag, self.source, self.payloads, notes_only=True)
        proof = self.hardware(True); proof['network_ota_install_verified'] = False
        with self.assertRaises(ValueError): self.publisher.validate_hardware(proof, self.tag, self.source, self.payloads, notes_only=True)

    def test_candidate_upload_is_draft_until_complete_stable_and_idempotent(self):
        api = FakeGitHub(); final = self.publish(api)
        self.assertFalse(final['draft']); self.assertFalse(final['prerelease'])
        self.assertTrue(final['body'].startswith(self.publisher.CANDIDATE_MARKER))
        uploads = [call for call in api.calls if call[3]]
        self.assertEqual([call[2] for call in uploads], [data for _, data, _ in self.payloads])
        self.assertEqual([call[2] for call in api.calls if call[0] == 'PATCH'],
                         [{'body': final['body'], 'draft': False, 'prerelease': False, 'make_latest': 'true'}])
        api.calls.clear(); self.publish(api); self.assertEqual(api.mutations(), [])

    def test_failed_upload_stays_draft_and_retry_only_uploads_missing_assets(self):
        api = FakeGitHub(); api.fail_upload = self.payloads[1][0]
        with self.assertRaises(RuntimeError): self.publish(api)
        self.assertTrue(api.release['draft']); self.assertEqual(len(api.release['assets']), 1)
        self.assertFalse(any(call[0] == 'PATCH' for call in api.calls))
        api.fail_upload = None; api.calls.clear(); final = self.publish(api)
        self.assertFalse(final['draft']); self.assertEqual(len([c for c in api.calls if c[3]]), 3)

    def test_existing_assets_cannot_be_replaced_or_extend_the_allowlist(self):
        baseline = self.publish(FakeGitHub())
        for change in ('digest', 'size', 'extra', 'duplicate', 'missing'):
            release = copy.deepcopy(baseline)
            if change == 'digest': release['assets'][0]['digest'] = 'sha256:' + '0' * 64
            elif change == 'size': release['assets'][0]['size'] += 1
            elif change == 'extra': release['assets'].append({'name': 'private-backup.bin', 'size': 1, 'digest': 'sha256:' + '0' * 64})
            elif change == 'duplicate': release['assets'].append(copy.deepcopy(release['assets'][0]))
            else: release['assets'].pop()
            api = FakeGitHub(release)
            with self.subTest(change=change), self.assertRaises(ValueError): self.publish(api)
            self.assertEqual(api.mutations(), [])

    def test_notes_only_acceptance_patches_only_body_and_is_idempotent(self):
        candidate = self.publish(FakeGitHub()); api = FakeGitHub(candidate)
        final = self.publish(api, mode='hardware', notes_only=True)
        self.assertTrue(final['body'].startswith(self.publisher.HARDWARE_MARKER))
        self.assertEqual(api.mutations(), [('PATCH', '/releases/123', {'body': final['body']}, False)])
        self.assertEqual(final['assets'], candidate['assets'])
        self.assertEqual(final['tag_name'], candidate['tag_name'])
        api.calls.clear(); self.publish(api, mode='hardware', notes_only=True); self.assertEqual(api.mutations(), [])
        api.calls.clear()
        with self.assertRaises(ValueError): self.publish(api)
        self.assertEqual(api.mutations(), []); self.assertEqual(api.release['body'], final['body'])

    def test_notes_only_never_creates_repairs_or_promotes_a_release(self):
        candidate = self.publish(FakeGitHub())
        variants = [None]
        for key, value in [('draft', True), ('prerelease', True), ('tag_name', 'v0.10.0')]:
            altered = copy.deepcopy(candidate); altered[key] = value; variants.append(altered)
        altered = copy.deepcopy(candidate); altered['assets'].pop(); variants.append(altered)
        altered = copy.deepcopy(candidate); altered['assets'][1]['digest'] = 'sha256:' + '0' * 64; variants.append(altered)
        for release in variants:
            api = FakeGitHub(release)
            with self.subTest(release=release), self.assertRaises(ValueError): self.publish(api, mode='hardware', notes_only=True)
            self.assertEqual(api.mutations(), [])
        api = FakeGitHub(candidate)
        with self.assertRaises(ValueError): self.publish(api, notes_only=True)
        self.assertEqual(api.mutations(), [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
