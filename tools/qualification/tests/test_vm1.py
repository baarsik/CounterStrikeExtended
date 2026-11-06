import base64
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from vm1 import build_command


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


if __name__ == "__main__":
    unittest.main()
