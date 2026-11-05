#!/usr/bin/env python3
"""Prepare a pending OTA release from this create-event's successful CI jobs.

No device access. The only remote mutation here is creating an absent, exact
lightweight tag after complete local validation. Publication uses the existing
publisher; public verification deliberately sends no authentication.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

import publish_github_release as publisher

ROOT = Path(__file__).resolve().parents[1]
REPO = publisher.REPO
require = publisher.require


def event_source(env, event, version):
    require(env.get('GITHUB_ACTIONS') == 'true' and env.get('GITHUB_REPOSITORY') == REPO,
            'Release workflow is restricted to this repository in GitHub Actions')
    require(env.get('GITHUB_EVENT_NAME') == 'create' and event.get('ref_type') == 'branch',
            'Only explicit creation of a release branch may request publication')
    require(re.fullmatch(r'v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)', version),
            'Release VERSION must be canonical stable SemVer')
    branch = 'release/ota-' + version
    require(event.get('ref') == branch and env.get('GITHUB_REF') == 'refs/heads/' + branch,
            'Created release branch must exactly match VERSION')
    source = env.get('GITHUB_SHA', '')
    require(re.fullmatch(r'[0-9a-f]{40}', source), 'An immutable event source SHA is required')
    require(re.fullmatch(r'[1-9][0-9]*', env.get('GITHUB_RUN_ID', '')), 'Missing workflow run identity')
    return source, branch


def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()


def check_source(root, source, branch):
    require(git(root, 'rev-parse', 'HEAD') == source, 'Checkout differs from immutable event source')
    require(git(root, 'remote', 'get-url', 'origin').removesuffix('.git') == 'https://github.com/' + REPO,
            'Unexpected origin repository')
    subprocess.run(['git', 'fetch', '--no-tags', 'origin',
                    '+refs/heads/main:refs/remotes/origin/main'], cwd=root, check=True)
    subprocess.run(['git', 'merge-base', '--is-ancestor', source, 'origin/main'], cwd=root, check=True)
    ref = git(root, 'ls-remote', 'origin', 'refs/heads/' + branch).split()
    require(len(ref) == 2 and ref[0] == source, 'Release branch moved or disappeared after the request')
    publisher.candidate_inputs_match(root, source)


def context(root=ROOT):
    version = (root / 'VERSION').read_text(encoding='ascii').strip()
    event = publisher.strict_json(Path(os.environ['GITHUB_EVENT_PATH']).read_bytes(), 'Workflow event')
    source, branch = event_source(os.environ, event, version)
    check_source(root, source, branch)
    subprocess.run([sys.executable, str(root / 'scripts/check_release_version.py')], cwd=root, check=True)
    return version, source


def evidence(root, relative):
    raw = (root / relative).read_bytes()
    return {'path': relative, 'sha256': hashlib.sha256(raw).hexdigest()}


def artifact_ids(env):
    result = {name: env.get('RELEASE_' + name.upper() + '_ARTIFACT_ID', '')
              for name in ('checks', 'firmware', 'native')}
    require(all(re.fullmatch(r'[1-9][0-9]*', value) for value in result.values())
            and len(set(result.values())) == 3, 'Each successful dependency must provide its distinct artifact ID')
    return result


def validate_build_inputs(root, source, source_paths=None, source_file=None):
    inputs = publisher.strict_json((root / 'artifacts/input-manifest.json').read_bytes(), 'Build inputs')
    require(inputs.get('commit') == source, 'Firmware artifact came from another source commit')
    if source_paths is None:
        source_paths = git(root, 'ls-tree', '-r', '--name-only', source, '--',
                           'src', 'ci', 'tests', 'scripts', '.github', 'licenses', 'apply_update.py').splitlines()
    expected = {path for path in source_paths if '__pycache__' not in Path(path).parts}
    recorded = inputs.get('sha256')
    require(type(recorded) is dict and set(recorded) == expected, 'Build input manifest is incomplete or contains unexpected paths')
    if source_file is None:
        source_file = lambda path: subprocess.check_output(['git', 'show', f'{source}:{path}'], cwd=root)
    for path, digest in recorded.items():
        require(type(digest) is str and re.fullmatch(r'[0-9a-f]{64}', digest)
                and hashlib.sha256(source_file(path)).hexdigest() == digest,
                'Build input differs from the immutable source: ' + path)


def candidate_proof(root, version, source, payloads):
    validate_build_inputs(root, source)
    assets = publisher.artifact_bindings(payloads)
    proof = {
        'schema': 1, 'kind': 'ota-build-candidate', 'project': REPO,
        'version': version, 'source_commit': source, 'hardware_acceptance': 'pending',
        'assets': assets,
        'checks': {
            'final_build': {'source_commit': source, 'target': 'jc4880p4', 'exit_code': 0,
                            'firmware_sha256': assets[f'BoilerPanel-{version}-firmware.bin']['sha256'],
                            'log': evidence(root, 'artifacts/firmware-build.log')},
            'browser_tests': {'source_commit': source, 'test_script': publisher.BROWSER_TEST,
                              'report': evidence(root, 'artifacts/equipment-web-review/browser-result.json')},
        },
    }
    # exit_code=0 is derived from the successful firmware dependency in this run,
    # and its actual log is checked again here. No hardware fields are fabricated.
    publisher.validate_candidate(root, proof, version, source, payloads)
    return proof


def notes(root, version):
    changelog = (root / 'CHANGELOG.md').read_text(encoding='utf-8')
    section = re.search(r'^## ' + re.escape(version) + r'\b[^\n]*\n(.*?)(?=^## |\Z)',
                        changelog, re.M | re.S)
    require(section is not None and section[1].strip(), 'Missing release changelog')
    return section[1].strip() + '\n\nПроверки и target-сборка: https://github.com/' + REPO + '/actions/runs/' + os.environ['GITHUB_RUN_ID'] + '\n'


def validate_prepared(root, version, source):
    validate_build_inputs(root, source)
    payloads = publisher.load_payloads(root, root / 'artifacts' / f'BoilerPanel-{version}-COM3.zip', version, source)
    proof = publisher.strict_json((root / 'artifacts/ota-build-result.json').read_bytes(), 'Candidate proof')
    publisher.validate_candidate(root, proof, version, source, payloads)
    publisher.release_notes((root / 'artifacts/release-notes.md').read_text(encoding='utf-8'),
                            'candidate', version, source, proof)
    return payloads


def ensure_tag(api, version, source):
    ref = 'refs/tags/' + version
    existing = api('GET', '/git/ref/tags/' + version, allow404=True)
    if existing is None:
        api('POST', '/git/refs', {'ref': ref, 'sha': source})
        existing = api('GET', '/git/ref/tags/' + version)
    require(existing.get('ref') == ref and existing.get('object', {}).get('type') == 'commit'
            and existing['object'].get('sha') == source, 'Existing or created tag does not identify the exact source; never overwrite it')


def tag_api(token):
    def request(method, path, data=None, allow404=False):
        body = None if data is None else json.dumps(data).encode('utf-8')
        req = urllib.request.Request('https://api.github.com/repos/' + REPO + path, data=body, method=method,
                                     headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                                              'X-GitHub-Api-Version': '2022-11-28', 'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req, timeout=30) as response:
                return json.loads(response.read())
        except urllib.error.HTTPError as error:
            if allow404 and error.code == 404:
                return None
            raise RuntimeError(f'GitHub tag request failed with HTTP {error.code}; do not change repository permissions to bypass it') from None
    return request


def public_url(url):
    parsed = urllib.parse.urlsplit(url)
    require(parsed.scheme == 'https' and parsed.hostname in
            ('github.com', 'release-assets.githubusercontent.com', 'objects.githubusercontent.com')
            and parsed.port in (None, 443) and parsed.username is None and parsed.password is None
            and not parsed.fragment, 'Unexpected public asset redirect')


class PublicRedirects(urllib.request.HTTPRedirectHandler):
    max_redirections = 4

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        public_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def public_hash(url, size, opener=None):
    public_url(url)
    opener = opener or urllib.request.build_opener(PublicRedirects())
    request = urllib.request.Request(url, headers={'User-Agent': 'BoilerPanel-public-verification',
                                                  'Accept-Encoding': 'identity'})
    digest = hashlib.sha256()
    received = 0
    started = time.monotonic()
    with opener.open(request, timeout=30) as response:
        require(response.status == 200 and response.headers.get('Content-Range') is None
                and response.headers.get('Content-Encoding', 'identity') == 'identity', 'Unexpected public asset response')
        length = response.headers.get('Content-Length')
        require(length is None or length == str(size), 'Public asset Content-Length mismatch')
        while True:
            require(time.monotonic() - started < 90, 'Public asset download deadline exceeded')
            chunk = response.read(min(65536, size - received + 1))
            if not chunk:
                break
            received += len(chunk)
            require(received <= size, 'Public asset exceeds the expected size')
            digest.update(chunk)
    require(received == size, 'Public asset download was truncated')
    return digest.hexdigest()


def verify_public(payloads, version, source, download=public_hash):
    result = {'result': 'PASS', 'version': version, 'source_commit': source, 'hardware_acceptance': 'pending',
              'verified_at_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'authentication_sent': False, 'assets': {}}
    for name, data, _ in payloads:
        url = f'https://github.com/{REPO}/releases/download/{version}/{name}'
        actual = download(url, len(data))
        require(actual == hashlib.sha256(data).hexdigest(), 'Public asset SHA-256 mismatch: ' + name)
        result['assets'][name] = {'url': url, 'size': len(data), 'sha256': actual}
    return result


def save(root, name, value):
    (root / 'artifacts' / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('step', choices=('request', 'artifact-ids', 'prepare', 'tag', 'verify-public'))
    args = parser.parse_args()
    version, source = context()
    if args.step == 'request':
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
            output.write(f'version={version}\nsource={source}\n')
    elif args.step == 'artifact-ids':
        artifact_ids(os.environ)
    elif args.step == 'prepare':
        payloads = publisher.load_payloads(ROOT, ROOT / 'artifacts' / f'BoilerPanel-{version}-COM3.zip', version, source)
        proof = candidate_proof(ROOT, version, source, payloads)
        text = notes(ROOT, version)
        publisher.release_notes(text, 'candidate', version, source, proof)
        save(ROOT, 'ota-build-result.json', proof)
        (ROOT / 'artifacts/release-notes.md').write_text(text, encoding='utf-8')
        save(ROOT, 'candidate-preflight.json', {'source_commit': source, 'version': version,
             'run_id': os.environ['GITHUB_RUN_ID'], 'hardware_acceptance': 'pending',
             'artifact_ids': artifact_ids(os.environ), 'assets': publisher.artifact_bindings(payloads)})
    elif args.step == 'tag':
        validate_prepared(ROOT, version, source)  # All artifact/proof guards run before the first remote mutation.
        ensure_tag(tag_api(publisher.github_token()), version, source)
        subprocess.run(['git', 'fetch', '--no-tags', 'origin', f'refs/tags/{version}:refs/tags/{version}'], cwd=ROOT, check=True)
    else:
        require(not os.environ.get('GITHUB_TOKEN') and not os.environ.get('GH_TOKEN'), 'Public verification must not receive a token')
        payloads = validate_prepared(ROOT, version, source)
        save(ROOT, 'public-download-verification.json', verify_public(payloads, version, source))
    print(json.dumps({'step': args.step, 'version': version, 'source_commit': source, 'hardware_acceptance': 'pending'}))


if __name__ == '__main__':
    main()
