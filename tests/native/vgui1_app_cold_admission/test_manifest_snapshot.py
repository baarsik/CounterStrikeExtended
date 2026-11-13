#!/usr/bin/env python3
"""Actual producer/validator with copied evidence and a controlled inspector boundary."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import vgui1_app_cold_manifest as manifest


class ManifestSnapshotTests(unittest.TestCase):
    def setUp(self):
        source = os.environ.get('CSE_APP_COLD_EVIDENCE')
        if not source:
            self.skipTest('Set fresh exact actual evidence path')
        retained = os.environ.get('CSE_APP_COLD_SNAPSHOT_FIXTURES')
        if retained:
            self.root = Path(retained) / self._testMethodName
            self.root.mkdir(parents=True, exist_ok=False)
        else:
            temporary = tempfile.TemporaryDirectory(prefix='cse-app-cold-snapshot-')
            self.addCleanup(temporary.cleanup)
            self.root = Path(temporary.name)
        self.evidence = self.root / 'evidence'
        self.evidence.mkdir()
        for name in ('receipt.json', 'observations.json', 'metadata.h', 'evidence.h',
                     'accepted.stamp', 'metadata.stamp'):
            shutil.copyfile(Path(source) / name, self.evidence / name)
        self.receipt, reference = manifest.fresh_receipt(self.evidence)
        self.receipt_bytes = (self.evidence / 'receipt.json').read_bytes()
        self.observations_bytes = (self.evidence / 'observations.json').read_bytes()
        (self.root / 'initial-receipt.json').write_bytes(self.receipt_bytes)
        (self.root / 'initial-observations.json').write_bytes(self.observations_bytes)
        record = bytes(index % 256 for index in range(352))
        image = self.root / 'authored-image'
        image.write_bytes(record)
        inspector = self.root / 'controlled-inspector-marker'
        inspector.write_bytes(b'not executed: injected inspector result only\n')
        prefix = reference['typed']['prefix']
        role, role_family, profile_family, app_family = reference['metadata_facts']
        profile = manifest.common.fnv(prefix[6:11] + prefix[21:23] + [prefix[26]])
        # ABI arithmetic is covered by the independent encoding/real-reader gate.
        # This controlled result isolates file-generation binding in real produce().
        self.facts = {'bytes': len(record), 'metadata_offset': 0, 'cpu': 0x0100000c,
                      'uuid': '01' * 16, 'sha256': hashlib.sha256(record).hexdigest(),
                      'record_sha256': hashlib.sha256(record).hexdigest(),
                      'provenance': self.receipt['input_digest'], 'kind': 1,
                      'catalog': [288, 1, 10, prefix[6], prefix[9] * 8, prefix[7], 3, 0],
                      'tables': [[role_family, 0, role], [profile_family, 0, profile],
                                 [app_family, 0, reference['signature']]],
                      'install_name': str(image), 'dependencies': []}
        self.options = SimpleNamespace(output=self.root / 'manifest.json', evidence=self.evidence,
                                       image=image, inspector=inspector, kind='offer')
        self.options.output.write_text('OLD ACCEPTANCE')

    def inspector_result(self, action=None):
        def inspect(*arguments, **keywords):
            if action:
                action()
            return SimpleNamespace(stdout=json.dumps(self.facts).encode())
        return inspect

    def require_rejection(self, action=None):
        with mock.patch.object(manifest.subprocess, 'run', side_effect=self.inspector_result(action)):
            with self.assertRaisesRegex(RuntimeError, 'changed'):
                manifest.produce(self.options)
        self.assertFalse(self.options.output.exists(), 'no stale or mixed-generation manifest may survive')

    def refresh_receipt(self):
        # Authored, internally consistent generation B. This is not a second
        # compiler qualification; both snapshots use the actual validator.
        receipt = json.loads(self.receipt_bytes)
        receipt['preprocessed_hashes'][next(iter(receipt['preprocessed_hashes']))] = 'b' * 64
        keys = ('format', 'contexts', 'compilers', 'inputs', 'dependency_hashes',
                'effective_environment', 'compiles', 'fixture_only', 'preprocessed_hashes')
        receipt['input_digest'] = manifest.common.digest_bytes(manifest.common.stable({key: receipt[key] for key in keys}))
        reference = json.loads(self.observations_bytes)[0]
        header = manifest.evidence.metadata_header(reference, receipt['input_digest'],
            ROOT / 'src/public/cse_vgui1_app_cold_metadata.h')
        (self.evidence / 'metadata.h').write_text(header)
        receipt['metadata_header_sha256'] = hashlib.sha256(header.encode()).hexdigest()
        (self.evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        for name in ('accepted.stamp', 'metadata.stamp'):
            (self.evidence / name).write_text(receipt['input_digest'] + '\n')
        current, _ = manifest.fresh_receipt(self.evidence)
        self.assertNotEqual(current['input_digest'], self.receipt['input_digest'])

    def test_unchanged_generation_publishes_captured_hashes(self):
        with mock.patch.object(manifest.subprocess, 'run', side_effect=self.inspector_result()):
            manifest.produce(self.options)
        output = json.loads(self.options.output.read_text())
        self.assertEqual(output['receipt_sha256'], hashlib.sha256(self.receipt_bytes).hexdigest())
        self.assertEqual(output.get('observations_sha256'), hashlib.sha256(self.observations_bytes).hexdigest())
        self.assertEqual(output['image']['provenance'], self.receipt['input_digest'])
        self.assertFalse(output['cold_load_ready'])
        self.assertFalse(output['native_service_ready'])

    def test_receipt_generation_changes_during_inspection_reject(self):
        self.require_rejection(self.refresh_receipt)

    def test_receipt_bytes_change_during_inspection_reject(self):
        self.require_rejection(lambda: (self.evidence / 'receipt.json').write_bytes(self.receipt_bytes + b'\n'))

    def test_observations_bytes_change_during_inspection_reject(self):
        self.require_rejection(lambda: (self.evidence / 'observations.json').write_bytes(self.observations_bytes + b'\n'))

    def test_reference_changes_during_inspection_reject(self):
        def change_reference():
            observations = json.loads(self.observations_bytes)
            observations[0]['controlled_refresh_note'] = 'different validated reference'
            (self.evidence / 'observations.json').write_text(json.dumps(observations, indent=2) + '\n')
            _, reference = manifest.fresh_receipt(self.evidence)
            self.assertIn('controlled_refresh_note', reference)
        self.require_rejection(change_reference)

    def initial_capture_interleaving(self, name, original):
        decode = json.loads
        changed = False

        def decode_then_refresh(value, *arguments, **keywords):
            nonlocal changed
            result = decode(value, *arguments, **keywords)
            encoded = value.encode() if isinstance(value, str) else value
            if not changed and encoded == original:
                changed = True
                (self.evidence / name).write_bytes(original + b'\n')
            return result

        with mock.patch.object(manifest.json, 'loads', side_effect=decode_then_refresh):
            self.require_rejection()
        self.assertTrue(changed, 'the controlled refresh occurred after decoding the validated bytes')

    def test_initial_receipt_capture_is_bound_to_decoded_bytes(self):
        self.initial_capture_interleaving('receipt.json', self.receipt_bytes)

    def test_initial_observations_capture_is_bound_to_decoded_bytes(self):
        self.initial_capture_interleaving('observations.json', self.observations_bytes)

    def test_receipt_hash_is_not_an_unbound_later_path_read(self):
        digest = manifest.common.file_digest

        def refresh_at_unbound_read(path):
            if Path(path) == self.evidence / 'receipt.json':
                # Only a later path-hash read reaches this controlled change.
                # It must not supply the hash of bytes different from validation.
                (self.evidence / 'receipt.json').write_bytes(self.receipt_bytes + b'\n')
            return digest(path)

        with mock.patch.object(manifest.common, 'file_digest', side_effect=refresh_at_unbound_read), \
                mock.patch.object(manifest.subprocess, 'run', side_effect=self.inspector_result()):
            manifest.produce(self.options)
        output = json.loads(self.options.output.read_text())
        self.assertEqual(output['receipt_sha256'], hashlib.sha256(self.receipt_bytes).hexdigest())


if __name__ == '__main__':
    unittest.main()
