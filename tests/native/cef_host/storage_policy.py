"""Inspect actual macOS host storage selection without initializing CEF or accessing Keychain."""
import argparse
import json
from pathlib import Path
import plistlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--release', type=Path, required=True)
parser.add_argument('--debug', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
records = []


def inspect(executable, root, name, succeeds=True):
    result = subprocess.run([str(executable), '--cse-browser-host', '--cse-storage=' + str(root),
                             '--cse-inspect-storage-policy'], input=b'', capture_output=True, timeout=30)
    (args.evidence / (name + '.stdout')).write_bytes(result.stdout)
    (args.evidence / (name + '.stderr')).write_bytes(result.stderr)
    records.append({'case': name, 'exit': result.returncode})
    assert not root.exists(), 'inspection must not create browser storage'
    if not succeeds:
        assert result.returncode != 0, 'invalid code identity must fail before Keychain access'
        return None
    assert result.returncode == 0, result.stderr.decode(errors='replace')
    return dict(line.split('=', 1) for line in result.stdout.decode().splitlines() if '=' in line)


with tempfile.TemporaryDirectory(prefix='CSE storage policy ') as temporary:
    owned = Path(temporary)
    requested = owned / 'Unicode β storage with spaces'
    release = args.release.resolve(strict=True)
    debug = args.debug.resolve(strict=True)
    first = inspect(release, requested, 'release-first')
    again = inspect(release, requested, 'release-repeat')
    other = inspect(debug, requested, 'debug')
    assert first == again, 'same valid code identity must reuse its encrypted development store'
    for values, flavor in [(first, 'release'), (other, 'debug')]:
        root = Path(values['cef_host_storage_root'])
        assert root.parent == requested / 'development' / flavor
        fingerprint = root.name
        assert len(fingerprint) == 64 and all(x in '0123456789abcdef' for x in fingerprint)
        assert values['cef_host_storage_cache'] == str(root / 'profile')
        assert values['cef_host_storage_service'] == (
            'CounterStrikeExtended Browser Development ' + flavor + ' ' + fingerprint)
    assert first['cef_host_storage_service'] != other['cef_host_storage_service']
    changed_app = owned / 'Changed host.app'
    subprocess.run(['/bin/cp', '-cR', str(release.parents[2]), str(changed_app)], check=True)
    changed_executable = changed_app / 'Contents/MacOS/cse_cef_host'
    relocated = inspect(changed_executable, requested, 'relocated-valid-code')
    assert relocated['cef_host_storage_service'] != first['cef_host_storage_service']
    subprocess.run(['/usr/bin/codesign', '--force', '--sign', '-', '--identifier',
                    'org.counterstrikeextended.cefhost.fixture.changed', str(changed_app)],
                   check=True, capture_output=True)
    changed = inspect(changed_executable, requested, 'changed-valid-code')
    assert changed['cef_host_storage_service'] != first['cef_host_storage_service']
    assert changed['cef_host_storage_root'] != first['cef_host_storage_root']
    assert changed['cef_host_storage_service'] != relocated['cef_host_storage_service']
    assert changed['cef_host_storage_root'] != relocated['cef_host_storage_root']
    plist = changed_app / 'Contents/Info.plist'
    value = plistlib.loads(plist.read_bytes())
    value['CFBundleDisplayName'] = 'Unsealed fixture change'
    plist.write_bytes(plistlib.dumps(value))
    inspect(changed_executable, requested, 'invalid-signature', succeeds=False)

(args.evidence / 'result.json').write_text(json.dumps({
    'passed': True, 'cases': records, 'cef_initialized': False, 'keychain_accessed': False,
    'signed_distribution_continuity_tested': False}, indent=2) + '\n')
print('Actual storage policy: identity reuse/change, profile separation, Unicode paths and invalid signature passed')
