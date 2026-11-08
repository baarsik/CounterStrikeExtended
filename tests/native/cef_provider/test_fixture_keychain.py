"""Check temporary identity ownership without reading or modifying a real Keychain."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from fixture_keychain import delete_fixture_keychain, fixture_identifier


class FixtureKeychainTests(unittest.TestCase):
    def test_identity_changes_with_host_and_fixture_but_survives_restart(self):
        with tempfile.TemporaryDirectory() as directory:
            host = Path(directory) / 'host'
            evidence = Path(directory) / 'evidence'
            host.write_bytes(b'first executable')
            first = fixture_identifier(host, evidence)
            self.assertEqual(first, fixture_identifier(host, evidence))
            self.assertNotEqual(first, fixture_identifier(host, evidence / 'other'))
            host.write_bytes(b'changed executable')
            self.assertNotEqual(first, fixture_identifier(host, evidence))
            self.assertRegex(first, r'^[0-9a-f]{64}$')

    def test_cleanup_deletes_only_exact_fixture_service_and_account(self):
        identifier = 'a' * 64
        with patch('fixture_keychain.subprocess.run', return_value=subprocess.CompletedProcess([], 0, '', '')) as run:
            self.assertTrue(delete_fixture_keychain(identifier)['deleted'])
        self.assertEqual(run.call_args.args[0], ['/usr/bin/security', 'delete-generic-password', '-s',
                         'CounterStrikeExtended Browser Fixture ' + identifier, '-a', 'CounterStrikeExtended Fixture'])

    def test_malformed_identity_cannot_reach_security(self):
        with patch('fixture_keychain.subprocess.run') as run:
            for identifier in ('', '../production', 'a' * 63, 'a' * 65, 'A' * 64):
                with self.assertRaises(ValueError):
                    delete_fixture_keychain(identifier)
            run.assert_not_called()

    def test_cleanup_failure_is_not_reported_as_deleted(self):
        with patch('fixture_keychain.subprocess.run', return_value=subprocess.CompletedProcess([], 44, '', 'missing')):
            self.assertFalse(delete_fixture_keychain('b' * 64)['deleted'])


if __name__ == '__main__':
    unittest.main()
