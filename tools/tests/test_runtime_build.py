"""Exercise runtime layout/staging and evaluated Release compile/link policy."""
from pathlib import Path
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('runtime_translator', ROOT / 'tools/translate_msbuild.py')
translator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(translator)


class RuntimeBuildTests(unittest.TestCase):
    def run_cmake(self, *arguments, cwd=None):
        result = subprocess.run(['cmake', *map(str, arguments)], cwd=cwd, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_game_relative_outputs_and_deleted_sdl_are_restored(self):
        # Native host libraries exercise the layout/build graph, not Windows ABI.
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            output = Path(directory) / 'game'
            source.mkdir()
            (source / 'stub.c').write_text('int probe(void) { return 0; }\n')
            (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(RuntimeLayout C)
set(CSE_SDK_DIR "{ROOT}/GoldSrc-SDK")
set(CSE_OUTPUT_DIRECTORY "{output}")
include("{ROOT}/cmake/RuntimeTargets.cmake")
foreach(game cstrike valve)
  cse_add_runtime(${{game}} SHARED client)
  target_sources(${{game}} PRIVATE stub.c)
  cse_runtime_directory(${{game}} "${{game}}/cl_dlls")
endforeach()
set(CMAKE_SIZEOF_VOID_P 4)
set(SDL_CPU_X86 1)
include("{ROOT}/cmake/SDL3Dependency.cmake")
add_library(provider_consumer STATIC stub.c)
cse_link_sdl3(provider_consumer Release)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/outputs.txt" CONTENT
  "$<TARGET_FILE:cstrike>\\n$<TARGET_FILE:valve>\\n$<TARGET_LINKER_FILE:cstrike>\\n")
''')
            self.run_cmake('-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
            self.run_cmake('--build', build, '--parallel', '2')
            cs, hl, import_file = (build / 'outputs.txt').read_text().splitlines()
            self.assertEqual(Path(cs).parent, output / 'cstrike/cl_dlls')
            self.assertEqual(Path(hl).parent, output / 'valve/cl_dlls')
            self.assertTrue(Path(cs).is_file())
            self.assertTrue(Path(hl).is_file())
            if sys.platform == 'win32':
                self.assertTrue(Path(import_file).is_relative_to(build / 'lib'))
            staged = output / 'SDL3.dll'
            expected = (ROOT / 'GoldSrc-SDK/external/SDL3/lib/x86/SDL3.dll').read_bytes()
            self.assertEqual(staged.read_bytes(), expected)
            staged.unlink()
            self.run_cmake('--build', build, '--target', 'provider_consumer')
            self.assertEqual(staged.read_bytes(), expected)
            self.assertFalse(list(output.rglob('*.pdb')))
            self.assertFalse(list(output.rglob('*.map')))

    def test_evaluated_release_diagnostics_require_opt_in(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk = root / 'GoldSrc-SDK'
            sdk.mkdir()
            shutil.copytree(ROOT / 'GoldSrc-SDK/external/SDL3', sdk / 'external/SDL3')
            (sdk / 'stub.c').write_text('int probe(void) { return 0; }\n')
            (sdk / 'fixture.vcxproj').write_text('''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><ConfigurationType>DynamicLibrary</ConfigurationType><LinkIncremental>false</LinkIncremental></PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile><RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary><DebugInformationFormat>ProgramDatabase</DebugInformationFormat></ClCompile>
    <Link><GenerateDebugInformation>true</GenerateDebugInformation><GenerateMapFile>true</GenerateMapFile></Link>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="stub.c"><AdditionalOptions>/Z7</AdditionalOptions></ClCompile></ItemGroup>
</Project>''')
            (root / 'docs/msbuild').mkdir(parents=True)
            with patch.multiple(translator, ROOT=root, SDK=sdk, TARGETS={'fixture': 'fixture'}, CASE_PATHS={}):
                translator.main()
            (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(ReleasePolicy C)
set(CSE_SDK_DIR "{sdk}")
set(MSVC TRUE)
include("{ROOT}/cmake/RuntimeTargets.cmake")
add_subdirectory(cmake/targets/fixture)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/link.txt" CONTENT "$<TARGET_PROPERTY:fixture,LINK_OPTIONS>" TARGET fixture)
''')
            for configuration, diagnostics in [('Release', 'OFF'), ('Release', 'ON'), ('Debug', 'OFF')]:
                with self.subTest(configuration=configuration, diagnostics=diagnostics):
                    build = root / f'{configuration}-{diagnostics}'
                    self.run_cmake('-S', root, '-B', build, '-G', 'Ninja', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                                   f'-DCMAKE_BUILD_TYPE={configuration}', f'-DCSE_RELEASE_DIAGNOSTICS={diagnostics}')
                    command = json.loads((build / 'compile_commands.json').read_text())[0]['command'].split()
                    link = (build / 'link.txt').read_text().split(';')
                    if configuration == 'Debug':
                        databases = [Path(flag.removeprefix('/ILK:')) for flag in link if flag.startswith('/ILK:')]
                        self.assertEqual(len(databases), 1)
                        self.assertTrue(databases[0].is_relative_to(build))
                        self.assertEqual(databases[0].name, 'fixture-Debug.ilk')
                    else:
                        self.assertFalse(any(flag.startswith('/ILK:') for flag in link))
                    if configuration == 'Release' and diagnostics == 'OFF':
                        self.assertNotIn('/Zi', command)
                        self.assertNotIn('/Z7', command)
                        self.assertNotIn('/DEBUG', link)
                        self.assertFalse(any(flag.startswith('/MAP:') for flag in link))
                    else:
                        self.assertIn('/Zi', command)
                        self.assertIn('/Z7', command)
                        self.assertIn('/DEBUG', link)
                        self.assertTrue(any(flag.startswith('/MAP:') for flag in link))

    def test_decoder_staging_and_distribution_materials_require_opt_in(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            output = Path(directory) / 'game'
            source.mkdir()
            (source / 'stub.c').write_text('int probe(void) { return 0; }\n')
            (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(DecoderStaging C)
set(CSE_ROOT "{ROOT}")
set(CSE_SDK_DIR "{ROOT}/GoldSrc-SDK")
set(CSE_OUTPUT_DIRECTORY "{output}")
include("{ROOT}/cmake/Mpg123Dependency.cmake")
add_library(consumer STATIC stub.c)
cse_link_mpg123(consumer Release)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/decoder.txt" CONTENT "$<TARGET_FILE:libmpg123>")
''')
            self.run_cmake('-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
            self.run_cmake('--build', build, '--target', 'consumer', '--parallel', '2')
            decoder = Path((build / 'decoder.txt').read_text())
            self.assertEqual(decoder.parent, output)
            self.assertTrue(decoder.is_file())
            self.assertFalse((output / 'licenses').exists())
            self.assertFalse(list(output.rglob('*.bz2')))
            decoder.unlink()
            self.run_cmake('--build', build, '--target', 'consumer', '--parallel', '2')
            self.assertTrue(decoder.is_file())
            self.run_cmake('-S', source, '-B', build, '-DCSE_STAGE_DISTRIBUTION_MATERIALS=ON')
            self.run_cmake('--build', build, '--target', 'consumer', '--parallel', '2')
            for name in ['COPYING', 'AUTHORS', 'UPSTREAM.md', 'mpg123-1.33.7.tar.bz2']:
                self.assertEqual((output / 'licenses/mpg123' / name).read_bytes(),
                                 (ROOT / 'GoldSrc-SDK/external/mpg123' / name).read_bytes())

    def test_ninja_presets_leave_old_generator_caches_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            shutil.copyfile(ROOT / 'CMakePresets.json', source / 'CMakePresets.json')
            (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
project(PresetContract NONE)
file(WRITE "${CMAKE_BINARY_DIR}/configuration.txt"
  "${CMAKE_GENERATOR}\\n${CMAKE_BUILD_TYPE}\\n${CSE_BUILD_DEDICATED}\\n${CSE_BUILD_BROADCASTING}\\n${CSE_BUILD_A3D}\\n")
''')
            old = source / 'build/runtime-x86-release'
            self.run_cmake('-S', source, '-B', old, '-G', 'Ninja Multi-Config')
            previous_cache = (old / 'CMakeCache.txt').read_bytes()
            for preset in ['default', 'runtime-x86-release']:
                self.run_cmake('--preset', preset, cwd=source)
                self.run_cmake('--build', '--preset', preset, cwd=source)
                self.assertEqual((source / 'build/release/configuration.txt').read_text(),
                                 'Ninja\nRelease\nON\nON\nON\n')
            self.run_cmake('--preset', 'runtime-x86-debug', cwd=source)
            self.assertEqual((source / 'build/debug/configuration.txt').read_text(),
                             'Ninja\nDebug\nON\nON\nON\n')
            self.assertEqual((old / 'CMakeCache.txt').read_bytes(), previous_cache)


if __name__ == '__main__':
    unittest.main()
