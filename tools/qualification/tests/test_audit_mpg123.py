"""The migrated runtime must neither require nor retain a Miles provider."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1]))
SPEC = importlib.util.spec_from_file_location('audit_mpg123', Path(__file__).parents[1] / 'audit_mpg123.py')


class MusicClosureTests(unittest.TestCase):
    def test_miles_modules_and_imports_are_rejected(self):
        audit = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(audit)
        for filename in ['Mss32.dll', 'mssmp3.asi', 'mssvoice.asi', 'binkawin.asi']:
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / filename).write_bytes(b'old provider')
                with self.assertRaisesRegex(ValueError, 'Miles'):
                    audit.require_no_miles(root, {'binaries': {}})
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'Miles'):
                audit.require_no_miles(Path(directory), {'binaries': {
                    'hw.dll': {'imports': ['Mss32.dll']}, 'swds.dll': {'imports': ['mpg123.dll']},
                    'mpg123.dll': {'imports': ['KERNEL32.dll']}}})

    def test_both_engines_must_use_the_staged_decoder(self):
        audit = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(audit)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report = {'binaries': {'hw.dll': {'imports': ['mpg123.dll']},
                                  'swds.dll': {'imports': ['mpg123.dll']},
                                  'mpg123.dll': {'imports': ['KERNEL32.dll']}}}
            audit.require_no_miles(root, report)
            report['binaries']['swds.dll']['imports'] = []
            with self.assertRaisesRegex(ValueError, 'swds.dll'):
                audit.require_no_miles(root, report)


if __name__ == '__main__':
    unittest.main()
