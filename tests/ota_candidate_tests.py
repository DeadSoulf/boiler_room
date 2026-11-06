"""Candidate pipeline fault checks; isolated fixtures, no network or device I/O."""
from pathlib import Path
import copy
import hashlib
import io
import json
import re
import subprocess
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import prepare_ota_candidate as candidate
import publish_github_release as publisher
import ota_release_tests as release_fixtures


class RequestTests(unittest.TestCase):
    def setUp(self):
        self.source, self.before = 'a' * 40, 'b' * 40
        self.version = 'v0.10.6'
        self.env = {'GITHUB_ACTIONS': 'true', 'GITHUB_REPOSITORY': publisher.REPO,
                    'GITHUB_EVENT_NAME': 'push', 'GITHUB_SHA': self.source,
                    'GITHUB_REF': 'refs/heads/main', 'GITHUB_RUN_ID': '123'}
        self.event = {'ref': 'refs/heads/main', 'before': self.before, 'after': self.source,
                      'deleted': False, 'forced': False, 'created': False,
                      'head_commit': {'id': self.source, 'message': candidate.request_message(self.version)}}

    def test_only_explicit_main_push_selects_immutable_source(self):
        self.assertEqual(candidate.event_source(self.env, self.event, self.version), (self.source, self.before))
        for field, value in [('GITHUB_ACTIONS', 'false'), ('GITHUB_REPOSITORY', 'someone/boiler_room'),
                             ('GITHUB_EVENT_NAME', 'create'), ('GITHUB_EVENT_NAME', 'pull_request'),
                             ('GITHUB_EVENT_NAME', 'workflow_dispatch'), ('GITHUB_REF', 'refs/heads/release/ota-v0.10.6'),
                             ('GITHUB_REF', 'refs/tags/v0.10.6'), ('GITHUB_SHA', 'main'),
                             ('GITHUB_SHA', 'A' * 40), ('GITHUB_SHA', '0' * 40), ('GITHUB_RUN_ID', '')]:
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                candidate.event_source(dict(self.env, **{field: value}), self.event, self.version)
        for field, value in [('ref', 'refs/heads/other'), ('before', '0' * 40), ('before', self.source),
                             ('before', 'main'), ('after', self.before), ('deleted', True), ('forced', True),
                             ('created', True), ('forced', None), ('head_commit', None),
                             ('head_commit', {'id': self.before, 'message': candidate.request_message(self.version)}),
                             ('head_commit', {'id': self.source, 'message': 'fix: ordinary version update'}),
                             ('head_commit', {'id': self.source, 'message': candidate.request_message('v0.10.5')}),
                             ('head_commit', {'id': self.source, 'message': candidate.request_message(self.version) + '\nextra'})]:
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                candidate.event_source(self.env, dict(self.event, **{field: value}), self.version)
        for key in self.event:
            event = copy.deepcopy(self.event);del event[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                candidate.event_source(self.env, event, self.version)

    def test_noncanonical_or_unsupported_version_is_refused(self):
        for version in ('v00.10.6', 'v0.10.6-rc1', '0.10.6', 'v0.10.6/extra', 'v0.10.6\nPOST',
                        'v4294967296.0.0', 'v4294967295.4294967295.0'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                candidate.event_source(self.env, self.event, version)

    def test_empty_or_ambiguous_ids_cannot_download_all_artifacts(self):
        valid = {'RELEASE_CHECKS_ARTIFACT_ID': '11', 'RELEASE_FIRMWARE_ARTIFACT_ID': '12',
                 'RELEASE_NATIVE_ARTIFACT_ID': '13'}
        self.assertEqual(candidate.artifact_ids(valid), {'checks': '11', 'firmware': '12', 'native': '13'})
        for value in ('', '0', '-1', '12', '11,12', '1\n2', 'None'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                candidate.artifact_ids(dict(valid, RELEASE_CHECKS_ARTIFACT_ID=value))

    def git_values(self):
        return {('rev-parse', 'HEAD'): self.source,
                ('remote', 'get-url', 'origin'): 'https://github.com/' + publisher.REPO + '.git',
                ('ls-remote', 'origin', 'refs/heads/main'): self.source + '\trefs/heads/main',
                ('rev-list', '--parents', '-n', '1', self.source): self.source + ' ' + self.before,
                ('show', '-s', '--format=%B', self.source): candidate.request_message(self.version),
                ('show', self.source + ':VERSION'): self.version,
                ('show', self.before + ':VERSION'): 'v0.10.5'}

    def test_fresh_main_and_exact_parent_version_message_are_checked(self):
        commands, values = [], self.git_values()
        with patch.object(candidate, 'git', side_effect=lambda root, *args: values[args]), patch.object(candidate.subprocess, 'run', side_effect=lambda argv, **kw: commands.append(argv)), patch.object(publisher, 'candidate_inputs_match') as inputs:
            candidate.check_source(ROOT, self.source, self.before, self.version)
            self.assertEqual(commands[0], ['git', 'fetch', '--no-tags', 'origin', '+refs/heads/main:refs/remotes/origin/main'])
            self.assertEqual(commands[1], ['git', 'merge-base', '--is-ancestor', self.source, 'origin/main'])
            inputs.assert_called_once_with(ROOT, self.source)
        wrong = [(("rev-parse", "HEAD"), self.before),
                 (("remote", "get-url", "origin"), 'https://github.com/other/repo'),
                 (("ls-remote", "origin", "refs/heads/main"), self.before + '\trefs/heads/main'),
                 (("ls-remote", "origin", "refs/heads/main"), self.source + '\trefs/tags/main'),
                 (("rev-list", "--parents", "-n", "1", self.source), self.source + ' ' + 'c' * 40),
                 (("rev-list", "--parents", "-n", "1", self.source), self.source + ' ' + self.before + ' ' + 'c' * 40),
                 (("show", "-s", "--format=%B", self.source), 'fix: ordinary commit'),
                 (("show", self.source + ':VERSION'), 'v0.10.7'),
                 (("show", self.before + ':VERSION'), self.version),
                 (("show", self.before + ':VERSION'), 'v0.11.0'),
                 (("show", self.before + ':VERSION'), 'v00.10.5')]
        for key, value in wrong:
            changed = dict(values);changed[key] = value
            with self.subTest(key=key, value=value), patch.object(candidate, 'git', side_effect=lambda root, *args: changed[args]), patch.object(candidate.subprocess, 'run'), patch.object(publisher, 'candidate_inputs_match') as inputs, self.assertRaises(ValueError):
                candidate.check_source(ROOT, self.source, self.before, self.version)
            inputs.assert_not_called()
        with patch.object(candidate, 'git', side_effect=lambda root, *args: values[args]), patch.object(candidate.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'merge-base')), self.assertRaises(subprocess.CalledProcessError):
            candidate.check_source(ROOT, self.source, self.before, self.version)

    def test_workflow_is_main_version_scoped_without_new_permissions(self):
        workflow = (ROOT / '.github/workflows/release-candidate.yml').read_text()
        self.assertIn('branches: [main]', workflow)
        self.assertIn('paths: [VERSION]', workflow)
        self.assertIn("github.event_name == 'push'", workflow)
        self.assertIn("github.ref == 'refs/heads/main'", workflow)
        self.assertNotIn('on: create', workflow)
        self.assertNotIn('workflow_dispatch:', workflow)
        self.assertIn('cancel-in-progress: false', workflow)
        before_publish, publish = workflow.split('  publish:', 1)
        self.assertNotIn('contents: write', before_publish)
        self.assertNotIn('GITHUB_TOKEN:', before_publish)
        self.assertIn('contents: write', publish)
        self.assertIn('needs: [request, ota, native]', publish)
        self.assertIn('ref: ${{ github.sha }}', workflow)


class RealGitRequestTests(unittest.TestCase):
    """Real local commit graphs; only remote fetch/ref reads are replaced."""
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.git_cmd('init', '--initial-branch=main')
        self.git_cmd('config', 'user.name', 'Release fixture')
        self.git_cmd('config', 'user.email', 'fixture@example.invalid')
        self.git_cmd('remote', 'add', 'origin', 'https://github.com/' + publisher.REPO + '.git')
        self.before = self.commit('v0.10.5', 'fixture base')
        self.source = self.commit('v0.10.6', candidate.request_message('v0.10.6'))
        self.remote_head = self.source
        self.git_cmd('update-ref', 'refs/remotes/origin/main', self.source)

    def git_cmd(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, text=True, stderr=subprocess.DEVNULL).strip()

    def commit(self, version, message):
        (self.root / 'VERSION').write_text(version + '\n')
        self.git_cmd('add', 'VERSION')
        self.git_cmd('commit', '--allow-empty', '-m', message)
        return self.git_cmd('rev-parse', 'HEAD')

    def check(self, source, before, version):
        original_git, original_run = candidate.git, subprocess.run
        def git(root, *args):
            if args == ('ls-remote', 'origin', 'refs/heads/main'):
                return self.remote_head + '\trefs/heads/main'
            return original_git(root, *args)
        def execute(argv, **kwargs):
            if argv[:2] == ['git', 'fetch']:
                return subprocess.CompletedProcess(argv, 0)
            return original_run(argv, **kwargs)
        with patch.object(candidate, 'git', side_effect=git), patch.object(candidate.subprocess, 'run', side_effect=execute):
            candidate.check_source(self.root, source, before, version)

    def test_exact_local_history_passes_and_remote_advance_stops(self):
        self.check(self.source, self.before, 'v0.10.6')
        self.remote_head = self.commit('v0.10.6', 'docs: later work')
        self.git_cmd('update-ref', 'refs/remotes/origin/main', self.remote_head)
        self.git_cmd('checkout', '--detach', self.source)
        with self.assertRaisesRegex(ValueError, 'Main changed'):
            self.check(self.source, self.before, 'v0.10.6')

    def test_real_multicommit_unchanged_version_and_dirty_input_refuse(self):
        next_source = self.commit('v0.10.7', candidate.request_message('v0.10.7'))
        self.remote_head = next_source
        self.git_cmd('update-ref', 'refs/remotes/origin/main', next_source)
        with self.assertRaisesRegex(ValueError, 'one direct commit'):
            self.check(next_source, self.before, 'v0.10.7')
        unchanged = self.commit('v0.10.7', candidate.request_message('v0.10.7'))
        self.remote_head = unchanged
        self.git_cmd('update-ref', 'refs/remotes/origin/main', unchanged)
        with self.assertRaisesRegex(ValueError, 'increase VERSION'):
            self.check(unchanged, next_source, 'v0.10.7')
        self.git_cmd('checkout', '--detach', self.source)
        self.remote_head = self.source
        self.git_cmd('update-ref', 'refs/remotes/origin/main', self.source)
        (self.root / 'VERSION').write_text('v0.10.99\n')
        with self.assertRaises(subprocess.CalledProcessError):
            self.check(self.source, self.before, 'v0.10.6')


class TokenTests(unittest.TestCase):
    def test_actions_uses_only_explicit_scoped_token_without_helper_or_output(self):
        env = {'GITHUB_ACTIONS': 'true', 'GITHUB_REPOSITORY': publisher.REPO, 'GITHUB_TOKEN': 'fixture-secret'}
        captured = io.StringIO()
        with patch.object(publisher.subprocess, 'run') as command, redirect_stdout(captured):
            self.assertEqual(publisher.github_token(env), 'fixture-secret')
            command.assert_not_called()
        self.assertEqual(captured.getvalue(), '')
        for change in ({'GITHUB_TOKEN': ''}, {'GITHUB_REPOSITORY': 'different/repo'}):
            with patch.object(publisher.subprocess, 'run') as command, self.assertRaises(ValueError):
                publisher.github_token(dict(env, **change))
            command.assert_not_called()

    def test_local_credential_helper_path_remains_available(self):
        with patch.object(publisher.subprocess, 'run', return_value=SimpleNamespace(returncode=0, stdout='username=fixture\npassword=local-fixture\n')) as command:
            self.assertEqual(publisher.github_token({}), 'local-fixture')
            self.assertEqual(command.call_args.args[0], ['git', 'credential', 'fill'])
            self.assertIn('path=DeadSoulf/boiler_room.git', command.call_args.kwargs['input'])


class PackageProofTests(unittest.TestCase):
    def setUp(self):
        fixture = release_fixtures.ReleaseTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.root, self.version, self.source = fixture.root, fixture.tag, fixture.commit
        fixture.write()
        self.files = {'firmware.bin': fixture.image, 'bootloader.bin': fixture.boot,
                      'partitions.bin': fixture.table, 'binary-verification.json': b'{}',
                      'scripts/flash_panel.py': b'# fixture', 'scripts/flash_COM3.cmd': b'@rem fixture\r\n',
                      'docs/flash-panel.md': b'fixture', 'START_HERE.txt': b'fixture',
                      'SOURCE.txt': ('GitHub commit: ' + self.source + '\n').encode()}
        self.package = self.root / 'artifacts' / f'BoilerPanel-{self.version}-COM3.zip'
        self.write_zip()
        self.committed = {publisher.WEB_HEADER: b'fixture web header', publisher.BROWSER_TEST: b'# fixture test'}
        for name, data in self.committed.items():
            (self.root / name).parent.mkdir(exist_ok=True, parents=True)
            (self.root / name).write_bytes(data)
        self.report = {'result': 'PASS', 'test_script': publisher.BROWSER_TEST,
                       'source_header_sha256': hashlib.sha256(self.committed[publisher.WEB_HEADER]).hexdigest(),
                       'test_script_sha256': hashlib.sha256(self.committed[publisher.BROWSER_TEST]).hexdigest(),
                       'widths': [320, 390, 768, 1280], 'page_errors': [], 'mutation_requests': [],
                       'physical_panel_contacted': False, 'status_polls': 28, 'coverage': ['fixture real-poll coverage']}
        (self.root / 'artifacts/equipment-web-review').mkdir()
        self.write_report()
        (self.root / 'artifacts/firmware-build.log').write_text('Processing jc4880p4\n[SUCCESS] Took 10 seconds\n')
        self.inputs = {'commit': self.source, 'sha256': {path: hashlib.sha256(data).hexdigest() for path, data in self.committed.items()}}
        self.write_inputs()

    def write_zip(self):
        files = dict(self.files)
        files['SHA256SUMS.json'] = json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in files.items()}).encode()
        with zipfile.ZipFile(self.package, 'w') as archive:
            for name, data in files.items():
                archive.writestr(f'BoilerPanel-{self.version}-COM3/' + name, data)

    def write_report(self):
        (self.root / 'artifacts/equipment-web-review/browser-result.json').write_text(json.dumps(self.report))

    def write_inputs(self):
        (self.root / 'artifacts/input-manifest.json').write_text(json.dumps(self.inputs))

    def payloads(self):
        return publisher.load_payloads(self.root, self.package, self.version, self.source)

    def proof(self):
        original = publisher.validate_candidate
        def validate(root, proof, version, source, payloads):
            return original(root, proof, version, source, payloads, source_file=self.committed.__getitem__)
        original_inputs = candidate.validate_build_inputs
        def validate_inputs(root, source):
            return original_inputs(root, source, self.committed.keys(), self.committed.__getitem__)
        with patch.object(publisher, 'validate_candidate', side_effect=validate), patch.object(candidate, 'validate_build_inputs', side_effect=validate_inputs):
            return candidate.candidate_proof(self.root, self.version, self.source, self.payloads())

    def test_existing_package_guard_is_shared_and_pending_proof_binds_exact_bytes(self):
        payloads = self.payloads()
        self.assertEqual(len(payloads), 4)
        self.assertEqual(payloads[0][1], self.package.read_bytes())
        proof = self.proof()
        self.assertEqual(proof['assets'], publisher.artifact_bindings(payloads))
        self.assertEqual(proof['hardware_acceptance'], 'pending')
        self.assertNotIn('readback_and_metadata_verified', proof)
        self.assertEqual(proof['checks']['final_build']['log']['sha256'], hashlib.sha256((self.root / 'artifacts/firmware-build.log').read_bytes()).hexdigest())

    def test_old_source_or_missing_current_build_evidence_is_refused(self):
        (self.root / 'artifacts/input-manifest.json').write_text(json.dumps({'commit': 'b' * 40}))
        with self.assertRaises(ValueError): self.proof()
        self.write_inputs()
        for log in ('', '[FAILED]\n', '[SUCCESS]\n[FAILED]\n'):
            (self.root / 'artifacts/firmware-build.log').write_text(log)
            with self.subTest(log=log), self.assertRaises(ValueError): self.proof()
        (self.root / 'artifacts/firmware-build.log').unlink()
        with self.assertRaises(FileNotFoundError): self.proof()

    def test_missing_added_and_wrong_input_hashes_cannot_claim_the_same_commit(self):
        original = dict(self.inputs['sha256'])
        for recorded in ({}, {**original, 'src/private-extra.h': '0' * 64},
                         {**original, publisher.WEB_HEADER: '0' * 64}):
            self.inputs['sha256'] = recorded
            self.write_inputs()
            with self.subTest(recorded=recorded), self.assertRaises(ValueError): self.proof()

    def test_previous_browser_report_cannot_be_relabelled_as_this_source(self):
        for key, value in (('result', 'REPRODUCED'), ('test_script_sha256', '0' * 64),
                           ('source_header_sha256', '0' * 64), ('physical_panel_contacted', True)):
            original = self.report[key]
            self.report[key] = value
            self.write_report()
            with self.subTest(key=key), self.assertRaises(ValueError): self.proof()
            self.report[key] = original

    def test_package_source_allowlist_and_internal_hashes_remain_strict(self):
        self.files['SOURCE.txt'] = ('GitHub commit: ' + 'b' * 40 + '\n').encode()
        self.write_zip()
        with self.assertRaises(ValueError): self.payloads()
        self.files['SOURCE.txt'] = ('GitHub commit: ' + self.source + '\n').encode()
        self.files['private-backup.bin'] = b'forbidden fixture'
        self.write_zip()
        with self.assertRaises(ValueError): self.payloads()
        del self.files['private-backup.bin']
        self.write_zip()
        with self.assertWarns(UserWarning), zipfile.ZipFile(self.package, 'a') as archive:
            archive.writestr(f'BoilerPanel-{self.version}-COM3/firmware.bin', b'wrong duplicate bytes')
        with self.assertRaises(ValueError): self.payloads()

    def test_corrupted_package_member_does_not_pass_the_original_hash_manifest(self):
        with zipfile.ZipFile(self.package) as archive:
            entries = {name: archive.read(name) for name in archive.namelist()}
        entries[f'BoilerPanel-{self.version}-COM3/firmware.bin'] = b'changed bytes'
        with zipfile.ZipFile(self.package, 'w') as archive:
            for name, data in entries.items(): archive.writestr(name, data)
        with self.assertRaisesRegex(ValueError, 'payload hash mismatch'): self.payloads()


class TagTests(unittest.TestCase):
    version, source = 'v0.10.3', 'a' * 40

    def test_absent_tag_is_created_once_then_read_back(self):
        calls = []
        def api(method, path, data=None, **kwargs):
            calls.append((method, path, data))
            if len(calls) == 1: return None
            return {'ref': 'refs/tags/' + self.version, 'object': {'type': 'commit', 'sha': self.source}}
        candidate.ensure_tag(api, self.version, self.source)
        self.assertEqual([c[0] for c in calls], ['GET', 'POST', 'GET'])
        self.assertEqual(calls[1][2], {'ref': 'refs/tags/' + self.version, 'sha': self.source})

    def test_existing_exact_tag_is_read_only_and_different_or_annotated_tag_refuses(self):
        for kind, sha in (('commit', self.source), ('commit', 'b' * 40), ('tag', self.source)):
            calls = []
            def api(*args, **kwargs):
                calls.append(args)
                return {'ref': 'refs/tags/' + self.version, 'object': {'type': kind, 'sha': sha}}
            if kind == 'commit' and sha == self.source:
                candidate.ensure_tag(api, self.version, self.source)
            else:
                with self.assertRaises(ValueError): candidate.ensure_tag(api, self.version, self.source)
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0][0], 'GET')

    def test_invalid_artifacts_stop_before_credentials_or_tag_mutation(self):
        with patch.object(sys, 'argv', ['prepare_ota_candidate.py', 'tag']), patch.object(candidate, 'context', return_value=(self.version, self.source)), patch.object(candidate, 'validate_prepared', side_effect=ValueError('bad proof')), patch.object(publisher, 'github_token') as token, patch.object(candidate, 'ensure_tag') as tag:
            with self.assertRaises(ValueError): candidate.main()
            token.assert_not_called()
            tag.assert_not_called()


class Response(io.BytesIO):
    status = 200
    def __init__(self, data, headers=None, status=200):
        super().__init__(data)
        self.headers, self.status = headers or {}, status


class DownloadTests(unittest.TestCase):
    url = 'https://github.com/DeadSoulf/boiler_room/releases/download/v0.10.3/firmware.bin'

    def opener(self, data, headers=None, status=200):
        def open_request(req, timeout):
            self.assertFalse(req.has_header('Authorization'))
            self.assertFalse(req.has_header('Cookie'))
            return Response(data, headers, status)
        return SimpleNamespace(open=open_request)

    def test_real_response_body_is_hashed_without_authentication(self):
        data = b'fixture public bytes'
        actual = candidate.public_hash(self.url, len(data), self.opener(data, {'Content-Length': str(len(data))}))
        self.assertEqual(actual, hashlib.sha256(data).hexdigest())

    def test_short_long_encoded_partial_and_wrong_length_downloads_refuse(self):
        for data, headers, status in ((b'ab', {}, 200), (b'abcd', {}, 200), (b'abc', {}, 206),
                                      (b'abc', {'Content-Range': 'bytes 0-2/3'}, 200),
                                      (b'abc', {'Content-Encoding': 'gzip'}, 200),
                                      (b'abc', {'Content-Length': '9'}, 200)):
            with self.subTest(data=data, headers=headers, status=status), self.assertRaises(ValueError):
                candidate.public_hash(self.url, 3, self.opener(data, headers, status))

    def test_redirect_allowlist_rejects_other_schemes_hosts_and_credentials(self):
        candidate.public_url('https://release-assets.githubusercontent.com/path?signature=fixture')
        for url in ('http://github.com/file', 'https://github.com.evil.invalid/file',
                    'https://github.com@evil.invalid/file', 'https://user:password@github.com/file',
                    'https://github.com:8443/file', 'https://example.invalid/file'):
            with self.subTest(url=url), self.assertRaises(ValueError): candidate.public_url(url)

    def test_public_delivery_report_never_claims_hardware_acceptance(self):
        payloads = [('a.bin', b'one', ''), ('b.zip', b'two', ''), ('manifest.json', b'{}', ''), ('SHA256SUMS.txt', b'fixture', '')]
        values = {name: data for name, data, _ in payloads}
        def download(url, size):
            data = values[url.rsplit('/', 1)[1]]
            self.assertEqual(size, len(data))
            return hashlib.sha256(data).hexdigest()
        report = candidate.verify_public(payloads, 'v0.10.3', 'a' * 40, download)
        self.assertEqual(report['result'], 'PASS')
        self.assertEqual(report['hardware_acceptance'], 'pending')
        self.assertIs(report['authentication_sent'], False)
        self.assertEqual(set(report['assets']), set(values))
        with self.assertRaises(ValueError):
            candidate.verify_public(payloads, 'v0.10.3', 'a' * 40, lambda *_: '0' * 64)


class ArtifactRetryTests(unittest.TestCase):
    def test_failed_job_retry_uses_new_immutable_names_and_preserves_output_ids(self):
        expected = {'github-ota.yml': ('github-ota-checks-', 'github-ota-esp32p4-'),
                    'calm-menu.yml': ('calm-menu-review-',)}
        for filename, prefixes in expected.items():
            workflow = (ROOT / '.github/workflows' / filename).read_text()
            for prefix in prefixes:
                name = re.search(r'^\s+name: (' + re.escape(prefix) + r'.+)$', workflow, re.M)
                self.assertIsNotNone(name)
                self.assertIn('${{ github.sha }}', name[1])
                self.assertIn('-attempt-${{ github.run_attempt }}', name[1])
            self.assertNotRegex(workflow, r'^\s+overwrite:\s*true', 'Retries must not replace immutable artifacts')
            self.assertIn('outputs.artifact-id', workflow)


if __name__ == '__main__':
    unittest.main(verbosity=2)
