#!/usr/bin/env python3
"""Compile-only actual-header tests; no App/module/provider execution."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / 'GoldSrc-SDK/public/cse_vgui1_app_cold_abi.h'


class FoundationTests(unittest.TestCase):
    def test_actual_private_members_without_class_rewriting(self):
        self.assertTrue(HEADER.is_file(), 'AppCold typed foundation is missing')
        with tempfile.TemporaryDirectory(prefix='cse-app-cold-types-') as work:
            command = [shutil.which('c++'), '-std=c++14', '-Wall', '-Wextra', '-Werror',
                       '-I' + str(ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui/include'),
                       '-I' + str(ROOT / 'GoldSrc-SDK/public'), '-c',
                       str(Path(__file__).with_name('private_access.cpp')), '-o', str(Path(work) / 'probe.o')]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
