"""Own a temporary encrypted Keychain identity for one macOS browser fixture."""
import hashlib
import re
import subprocess


FIXTURE_KEYCHAIN_PREFIX = 'CounterStrikeExtended Browser Fixture '
FIXTURE_KEYCHAIN_ACCOUNT = 'CounterStrikeExtended Fixture'


def fixture_identifier(host, evidence):
    return hashlib.sha256(host.read_bytes() + str(evidence).encode('utf-8')).hexdigest()


def delete_fixture_keychain(identifier):
    if re.fullmatch('[0-9a-f]{64}', identifier) is None:
        raise ValueError('Invalid fixture Keychain identifier')
    service = FIXTURE_KEYCHAIN_PREFIX + identifier
    result = subprocess.run(['/usr/bin/security', 'delete-generic-password', '-s', service,
                             '-a', FIXTURE_KEYCHAIN_ACCOUNT], capture_output=True, text=True, timeout=10)
    return {'service': service, 'account': FIXTURE_KEYCHAIN_ACCOUNT, 'return_code': result.returncode,
            'stderr': result.stderr.strip(), 'deleted': result.returncode == 0}
