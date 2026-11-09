"""A native dependency must reach both configs without entering Windows selections."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class NativeSdlPolicyTests(unittest.TestCase):
    def configs(self, debug='true', release='true'):
        return {name: {'properties': {'CseNativeSdl3': value}}
                for name, value in [('Debug', debug), ('Release', release)]}

    def test_native_dependency_applies_both_configs_and_never_windows(self):
        lines = []
        translator.emit_native_sdl3_dependency(lines, 'fixture', self.configs())
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            receipt = folder / 'calls.txt'
            script = folder / 'policy.cmake'
            script.write_text('function(cse_link_sdl3 target configuration)\n'
                'file(APPEND "${CSE_RECEIPT}" "${target}:${configuration}\\n")\n'
                'endfunction()\n' + '\n'.join(lines) + '\n')
            for windows in ('ON', 'OFF'):
                receipt.write_text('')
                result = subprocess.run(['cmake', f'-DWIN32={windows}', f'-DCSE_RECEIPT={receipt}',
                                         '-P', str(script)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                expected = [] if windows == 'ON' else ['fixture:Debug', 'fixture:Release']
                self.assertEqual(receipt.read_text().splitlines(), expected)

    def test_unrequested_targets_acquire_no_dependency(self):
        for configs in (self.configs('false', 'false'),
                        {name: {'properties': {}} for name in ('Debug', 'Release')}):
            lines = []
            translator.emit_native_sdl3_dependency(lines, 'fixture', configs)
            self.assertEqual(lines, [])

    def test_mismatched_or_invalid_policy_fails_before_generation(self):
        for configs in (self.configs('true', 'false'), self.configs('SDL3', 'SDL3')):
            with self.assertRaises(ValueError):
                translator.emit_native_sdl3_dependency([], 'fixture', configs)


if __name__ == '__main__':
    unittest.main()
