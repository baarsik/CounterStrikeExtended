#!/usr/bin/env python3
"""Negative tooling controls use copied inputs only; no original source is mutated."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import vgui1_app_cold_manifest as manifest


class ToolTests(unittest.TestCase):
    def setUp(self):
        directory = os.environ.get('CSE_APP_COLD_EVIDENCE')
        contexts = os.environ.get('CSE_APP_COLD_CONTEXTS')
        if not directory or not contexts:
            self.skipTest('Set fresh exact actual evidence and contexts paths')
        self.evidence = Path(directory)
        self.contexts = Path(contexts)
        self.temporary = tempfile.TemporaryDirectory(prefix='cse-app-cold-tools-')
        self.root = Path(self.temporary.name)

    def tearDown(self):
        if hasattr(self, 'temporary'):
            self.temporary.cleanup()

    def generate(self, contexts=None, extra=()):
        output = self.root / 'output'
        output.mkdir(exist_ok=True)
        for name in ('accepted.stamp', 'metadata.stamp', 'evidence.h', 'metadata.h'):
            (output / name).write_text('OLD INVALID ACCEPTANCE')
        command = [sys.executable, str(ROOT / 'tools/vgui1_app_cold_evidence.py'), 'generate',
            '--schema', str(ROOT / 'tools/vgui1_app_cold_evidence_schema.json'),
            '--foundation', str(ROOT / 'src/public/cse_vgui1_app_cold_abi.h'),
            '--contexts', str(contexts or self.contexts), '--output', str(output), '--metadata'] + list(extra)
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        for name in ('accepted.stamp', 'metadata.stamp', 'evidence.h', 'metadata.h'):
            self.assertFalse((output / name).exists(), name + ' must be revoked')
        return result.stderr

    def copied_evidence(self):
        output = self.root / 'copied-evidence'
        output.mkdir()
        for name in ('receipt.json', 'observations.json', 'metadata.h', 'evidence.h',
                     'accepted.stamp', 'metadata.stamp'):
            shutil.copyfile(self.evidence / name, output / name)
        return output

    def test_fresh_actual_metadata_is_not_cold_readiness(self):
        receipt, reference = manifest.fresh_receipt(self.evidence)
        self.assertTrue(receipt['metadata_ready'])
        self.assertFalse(receipt['publisher_ready'])
        self.assertFalse(receipt['cold_load_ready'])
        self.assertEqual(len(receipt['contexts']), 3)
        self.assertEqual(reference['typed']['prefix'][11], 664)

    def test_fixture_mode_can_never_publish(self):
        self.assertIn('never fixture-only', self.generate(extra=['--fixture-only']))

    def test_missing_actual_owner_revokes_output(self):
        data = json.loads(self.contexts.read_text())
        data['contexts'].pop()
        selected = self.root / 'missing-owner.json'
        selected.write_text(json.dumps(data))
        self.assertIn('actual hw/sw', self.generate(selected))

    def test_any_generated_header_injection_rejects(self):
        data = json.loads(self.contexts.read_text())
        database = json.loads(Path(data['compile_database']).read_text())
        injected = self.root / 'innocent-name.h'
        injected.write_text('namespace cse { namespace vgui1_app_cold_generated {} }\n')
        for row in database:
            if row['file'].endswith('/engine/vgui_intwrap.cpp'):
                arguments = shlex.split(row.pop('command'))
                row['arguments'] = arguments[:1] + ['-include', str(injected)] + arguments[1:]
        copy = self.root / 'copied-database.json'
        copy.write_text(json.dumps(database))
        data['compile_database'] = str(copy)
        selected = self.root / 'injected-contexts.json'
        selected.write_text(json.dumps(data))
        self.assertIn('No generated AppCold header', self.generate(selected))

    def test_manifest_failure_removes_prior_acceptance(self):
        copied = self.copied_evidence()
        (copied / 'metadata.stamp').unlink()
        prior = self.root / 'old-manifest.json'
        prior.write_text('OLD ACCEPTANCE')
        options = SimpleNamespace(output=prior, evidence=copied, image=self.root / 'unbuilt-image',
                                  inspector=self.root / 'unbuilt-inspector', kind='offer')
        with self.assertRaises(FileNotFoundError):
            manifest.produce(options)
        self.assertFalse(prior.exists())

    def test_compiler_environment_drift_rejects(self):
        with mock.patch.dict(os.environ, {'CPATH': str(self.root / 'changed-compiler-path')}):
            with self.assertRaisesRegex(RuntimeError, 'environment drift'):
                manifest.fresh_receipt(self.evidence)

    def test_metadata_header_guard_and_cpp14(self):
        source = self.root / 'guard.cpp'
        source.write_text('#include "' + str(self.evidence / 'metadata.h') + '"\n')
        command = ['c++', '-std=c++14', '-fno-exceptions', '-c', str(source),
                   '-o', str(self.root / 'guard.o')]
        denied = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(denied.returncode, 0)
        self.assertIn('only for the constant publisher', denied.stderr)
        accepted = subprocess.run(command + ['-DCSE_VGUI1_APP_COLD_METADATA_PUBLISHER=1'],
                                  capture_output=True, text=True, timeout=30)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)

    def test_missing_stamp_revokes_freshness(self):
        copied = self.copied_evidence()
        (copied / 'metadata.stamp').unlink()
        with self.assertRaises((RuntimeError, FileNotFoundError)):
            manifest.fresh_receipt(copied)

    def test_generated_header_drift_rejects(self):
        copied = self.copied_evidence()
        with (copied / 'metadata.h').open('a') as stream:
            stream.write('// changed\n')
        with self.assertRaisesRegex(RuntimeError, 'Generated header drift'):
            manifest.fresh_receipt(copied)

    def test_actual_input_drift_rejects(self):
        copied = self.copied_evidence()
        path = self.root / 'owned-input'
        path.write_text('after')
        receipt = json.loads((copied / 'receipt.json').read_text())
        receipt['inputs'][str(path)] = '0' * 64
        (copied / 'receipt.json').write_text(json.dumps(receipt))
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            manifest.fresh_receipt(copied)

    def test_provenance_mutation_rejects(self):
        copied = self.copied_evidence()
        receipt = json.loads((copied / 'receipt.json').read_text())
        receipt['compiles'][0]['arguments'].append('-DUNMEASURED_OPTION=1')
        (copied / 'receipt.json').write_text(json.dumps(receipt))
        with self.assertRaisesRegex(RuntimeError, 'provenance digest'):
            manifest.fresh_receipt(copied)


if __name__ == '__main__':
    unittest.main()
