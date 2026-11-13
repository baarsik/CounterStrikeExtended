"""Build a native SDL consumer from the pinned source archive; never run it."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from binary_identity import inspect_bytes, require_architecture

ROOT = Path(__file__).resolve().parents[2]


class SDL3SourceTests(unittest.TestCase):
    def test_source_provider_builds_and_restages_target_library(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            source.mkdir()
            (source / 'probe.c').write_text('#include <SDL3/SDL.h>\nint main(void) { return SDL_GetVersion() == 0; }\n')
            (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(SDLSourceProbe C CXX)
set(CSE_ROOT "{ROOT}")
set(CSE_SDK_DIR "{ROOT}/src")
set(CSE_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/runtime")
set(CSE_SDL3_FROM_SOURCE ON)
include("{ROOT}/cmake/TargetPlatform.cmake")
include("{ROOT}/cmake/SDL3Dependency.cmake")
add_executable(probe probe.c)
cse_link_sdl3(probe Release)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/provider.txt" CONTENT "$<TARGET_FILE:SDL3::SDL3-shared>")
''')
            configure = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                                        '-DCMAKE_BUILD_TYPE=Release'], capture_output=True, text=True)
            self.assertEqual(configure.returncode, 0, configure.stdout + configure.stderr)
            result = subprocess.run(['cmake', '--build', str(build), '--target', 'probe', '--parallel', '6'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout[-6000:] + result.stderr)
            provider = Path((build / 'provider.txt').read_text())
            # Use the compiler's output, not platform.machine() / the build host.
            identity = inspect_bytes((build / ('probe.exe' if sys.platform == 'win32' else 'probe')).read_bytes())
            self.assertEqual(require_architecture(provider.read_bytes(), identity['architectures'][0])['format'],
                             identity['format'])
            staged = build / 'runtime' / provider.name
            self.assertEqual(staged.read_bytes(), provider.read_bytes())
            staged.unlink()
            result = subprocess.run(['cmake', '--build', str(build), '--target', 'probe', '--parallel', '2'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(staged.read_bytes(), provider.read_bytes())
            self.assertFalse(list((build / 'runtime').rglob('*.a')))
            self.assertFalse(list((build / 'runtime').rglob('*.pdb')))
            self.assertFalse((build / 'runtime/licenses').exists())
            original = provider.read_bytes()
            try:
                provider.write_bytes(b'wrong architecture/provider')
                result = subprocess.run(['cmake', '--build', str(build), '--target', 'cse_stage_sdl3'],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, 'staging must recheck actual provider machine types')
                self.assertIn('architecture validation', result.stdout + result.stderr)
            finally:
                provider.write_bytes(original)



if __name__ == '__main__':
    unittest.main()
