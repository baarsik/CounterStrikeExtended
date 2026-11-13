#!/usr/bin/env python3
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
FOLDER = ROOT / 'src/utils/vgui/lib-src/vgui/posix'


class SourceBoundaryTests(unittest.TestCase):
    def test_approved_component_exists_without_selecting_old_stubs(self):
        for name in ('App.cpp', 'VGui1AppPlatform.h', 'VGui1AppPlatform.cpp', 'VGui1AppClock.cpp'):
            self.assertTrue((FOLDER / name).is_file(), 'Approved App component missing: ' + name)
        text = (FOLDER / 'App.cpp').read_text()
        self.assertNotIn('linux/App.cpp', text)
        self.assertNotIn('CFPreferences', text)
        self.assertNotIn('SDL_', text)


if __name__ == '__main__':
    unittest.main()
