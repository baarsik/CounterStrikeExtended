import base64
import re
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from vm1 import build_command, run_script


class VMCommandTests(unittest.TestCase):
    def test_transports_unicode_script_without_shell_interpolation(self):
        script = "Write-Output 'Zażółć ` $HOME'\n"
        command = build_command(script, vm="Windows 11")
        self.assertEqual(command[:5], ["prlctl", "exec", "Windows 11", "--current-user", "powershell.exe"])
        self.assertEqual(base64.b64decode(command[-1]).decode("utf-16-le"), script)

    def test_system_execution_is_explicit(self):
        command = build_command("Get-Date", vm="Named VM", system=True)
        self.assertEqual(command[:4], ["prlctl", "exec", "Named VM", "powershell.exe"])
        self.assertNotIn("--current-user", command)

    def test_large_script_uses_bounded_chunks_and_preserves_unicode_and_exit_status(self):
        script = "Write-Output 'Zażółć'\n" * 1000
        chunks = []
        cleaned = False

        def guest(command, **options):
            nonlocal cleaned
            self.assertLessEqual(len(command[-1]), 2800)
            self.assertGreater(options['timeout'], 0)
            text = base64.b64decode(command[-1]).decode('utf-16-le')
            payload = re.search(r"FromBase64String\('([^']*)'\)", text)
            if payload:
                chunks.append(base64.b64decode(payload[1]))
            if 'Remove-Item' in text:
                cleaned = True
            status = 7 if '-File $p' in text else 0
            return subprocess.CompletedProcess(command, status, 'script output' if status else '', '')

        result = run_script(script, runner=guest)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(result.stdout, 'script output')
        self.assertEqual(b''.join(chunks).decode('utf-8-sig'), script)
        self.assertTrue(cleaned)

    def test_staging_failure_still_cleans_owned_temp_file(self):
        cleaned = False

        def guest(command, **options):
            nonlocal cleaned
            self.assertLessEqual(len(command[-1]), 2800)
            text = base64.b64decode(command[-1]).decode('utf-16-le')
            if 'Remove-Item' in text:
                cleaned = True
                return subprocess.CompletedProcess(command, 0, '', '')
            return subprocess.CompletedProcess(command, 9, '', 'staging failed')

        result = run_script('x' * 2000, runner=guest)
        self.assertEqual(result.returncode, 9)
        self.assertEqual(result.stderr, 'staging failed')
        self.assertTrue(cleaned)


if __name__ == "__main__":
    unittest.main()
