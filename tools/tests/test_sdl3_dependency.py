"""Pinned provider and build contract regression tests (no runtime execution)."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('sdl3_dependency', ROOT / 'tools/sdl3_dependency.py')
sdl3 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdl3)


class PayloadTests(unittest.TestCase):
    def test_pinned_payload_identity(self):
        entries = sdl3.validate_payload(ROOT / 'src/external/SDL3')
        self.assertGreater(len(entries), 90)
        self.assertIn('lib/x86/SDL3.dll', entries)

    def test_missing_and_mutated_providers_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = Path(directory) / 'SDL3'
            shutil.copytree(ROOT / 'src/external/SDL3', payload)
            for relative in ['lib/x86/SDL3.lib', 'lib/x86/SDL3.dll', 'include/SDL3/SDL.h']:
                with self.subTest(relative=relative):
                    path = payload / relative
                    original = path.read_bytes()
                    path.unlink()
                    with self.assertRaisesRegex(ValueError, 'Missing SDL3 payload'):
                        sdl3.validate_payload(payload)
                    path.write_bytes(original + b'changed')
                    with self.assertRaisesRegex(ValueError, 'SDL3 hash mismatch'):
                        sdl3.validate_payload(payload)
                    path.write_bytes(original)

    def test_manifest_cannot_drop_pinned_files(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = Path(directory) / 'SDL3'
            shutil.copytree(ROOT / 'src/external/SDL3', payload)
            manifest = payload / 'SHA256SUMS'
            manifest.write_text('\n'.join(manifest.read_text().splitlines()[1:]) + '\n')
            with self.assertRaisesRegex(ValueError, 'manifest hash mismatch'):
                sdl3.validate_payload(payload)

    def test_cmake_validation_matches_provider_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory) / 'sdk'
            payload = sdk / 'external/SDL3'
            shutil.copytree(ROOT / 'src/external/SDL3', payload)
            command = ['cmake', '-DCSE_SDK_DIR=' + str(sdk), '-P', str(ROOT / 'cmake/SDL3Dependency.cmake')]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            for relative in ['lib/x86/SDL3.lib', 'lib/x86/SDL3.dll']:
                path = payload / relative
                original = path.read_bytes()
                path.unlink()
                failed = subprocess.run(command, capture_output=True, text=True)
                self.assertNotEqual(failed.returncode, 0)
                self.assertIn('Missing SDL3 payload', failed.stderr)
                path.write_bytes(original + b'changed')
                failed = subprocess.run(command, capture_output=True, text=True)
                self.assertNotEqual(failed.returncode, 0)
                self.assertIn('SDL3 hash mismatch', failed.stderr)
                path.write_bytes(original)


class StagingTests(unittest.TestCase):
    def test_single_always_run_stager_restores_deleted_dll(self):
        # Build-graph test only: the host compiler creates static archives. VM1
        # separately qualifies native Windows x86 shared-library linkage.
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            source.mkdir()
            (source / 'stub.c').write_text('int probe(void) { return 0; }\n')
            (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(StagingContract C)
set(CMAKE_SIZEOF_VOID_P 4)
set(SDL_CPU_X86 1)
set(CSE_SDK_DIR "{ROOT}/src")
include("{ROOT}/cmake/SDL3Dependency.cmake")
foreach(consumer probe client_probe)
  add_library(${{consumer}} STATIC stub.c)
  cse_link_sdl3(${{consumer}} Release)
  cse_link_sdl3(${{consumer}} Debug)
endforeach()
''')
            def run(*arguments):
                completed = subprocess.run(['cmake', *arguments], capture_output=True, text=True)
                self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
                return completed
            run('-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release')
            run('--build', str(build), '--target', 'probe', 'client_probe', '--parallel', '2')
            staged = build / 'runtime/Release/SDL3.dll'
            expected = (ROOT / 'src/external/SDL3/lib/x86/SDL3.dll').read_bytes()
            self.assertEqual(staged.read_bytes(), expected)
            staged.unlink()
            run('--build', str(build), '--target', 'client_probe')
            self.assertEqual(staged.read_bytes(), expected)


if __name__ == '__main__':
    unittest.main()
