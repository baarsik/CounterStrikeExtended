"""A source reference must be pinned, sealed and independent of working edits."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

MODULE = Path(__file__).parents[1] / 'capture_reference.py'
if MODULE.exists():
    spec = importlib.util.spec_from_file_location('capture_reference', MODULE)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
else:
    capture = None


class ReferenceCaptureTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(capture, 'reference capture implementation is required')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / 'repository'
        self.repo.mkdir()
        self.git('init', '-q')
        (self.repo / 'source.cpp').write_bytes(b'original source\n')
        self.git('add', 'source.cpp')
        self.git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                 'commit', '-qm', 'reference')
        self.revision = self.git('rev-parse', 'HEAD').strip()

    def git(self, *arguments):
        return subprocess.check_output(['git', '-C', str(self.repo), *arguments], text=True)

    def test_capture_uses_commit_contents_and_cannot_claim_runtime_qualification(self):
        (self.repo / 'source.cpp').write_bytes(b'uncommitted edits\n')
        report = capture.capture_source(self.repo, self.root / 'reference', self.revision)
        self.assertEqual(report['revision'], self.revision)
        self.assertEqual(report['evidence'], 'source-only')
        self.assertEqual(report['files']['source.cpp']['sha256'], hashlib.sha256(b'original source\n').hexdigest())
        self.assertEqual(capture.verify_reference(self.root / 'reference'), report)
        self.assertFalse(report['qualified'])

    def test_existing_destination_and_invalid_revision_fail_without_overwrite(self):
        destination = self.root / 'reference'
        destination.mkdir()
        sentinel = destination / 'owned.txt'
        sentinel.write_text('retain me')
        with self.assertRaisesRegex(ValueError, 'exists'):
            capture.capture_source(self.repo, destination, self.revision)
        self.assertEqual(sentinel.read_text(), 'retain me')
        with self.assertRaises(ValueError):
            capture.capture_source(self.repo, self.root / 'invalid', 'missing-revision')
        self.assertFalse((self.root / 'invalid').exists())

    def test_mutated_archive_or_manifest_is_rejected(self):
        destination = self.root / 'reference'
        capture.capture_source(self.repo, destination, self.revision)
        archive = destination / 'source.tar'
        archive.chmod(0o644)
        archive.write_bytes(archive.read_bytes() + b'changed')
        with self.assertRaisesRegex(ValueError, 'hash'):
            capture.verify_reference(destination)
        second = self.root / 'second'
        capture.capture_source(self.repo, second, self.revision)
        manifest = second / 'manifest.json'
        manifest.chmod(0o644)
        data = json.loads(manifest.read_text())
        data['qualified'] = True
        manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'hash'):
            capture.verify_reference(second)


if __name__ == '__main__':
    unittest.main()
