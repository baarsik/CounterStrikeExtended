"""Verify manifest membership and evaluated per-file compiler policy on a native host."""
from copy import deepcopy
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class PortableGameGenerationTests(unittest.TestCase):
    def test_both_compiler_branches_share_the_owning_manifest_source_inventory(self):
        records = json.loads((ROOT / 'docs/msbuild/repository-selections.json').read_text())
        games = [record for record in records if record['target'] in ('hl_server', 'cs_server')]
        self.assertEqual(len(games), 2)
        for game in games:
            target = game['target']
            source = (ROOT / f'cmake/targets/{target}/Sources.cmake').read_text()
            actual = re.findall(r'^  "([^"]+)"$', source, re.M)
            for config in ('Debug', 'Release'):
                items = game['configurations'][config]['items']
                expected = [translator.cmake_source(item) for item in items
                            if item['kind'] in ('ClCompile', 'ResourceCompile')
                            and item['metadata'].get('ExcludedFromBuild') != 'true']
                self.assertEqual(actual, expected, target + '/' + config)
            policy = (ROOT / f'cmake/targets/{target}/CMakeLists.txt').read_text()
            self.assertEqual(policy.count('include("${CMAKE_CURRENT_LIST_DIR}/Sources.cmake")'), 1)
            self.assertNotIn('target_sources(', policy)

    def test_evaluated_overrides_preserve_exceptions_floating_point_and_assertions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('ordinary.cpp', 'override.cpp'):
                (root / name).write_text('int value;\n')
            configs = {}
            for config in ('Debug', 'Release'):
                metadata = {'Optimization': 'Disabled' if config == 'Debug' else 'MaxSpeed',
                            'ExceptionHandling': 'false', 'RuntimeTypeInfo': 'true',
                            'FloatingPointModel': 'Precise', 'DebugInformationFormat': 'ProgramDatabase',
                            'PreprocessorDefinitions': 'WIN32;UTL_ENGINE_ASSERTS' + (';_DEBUG' if config == 'Debug' else '')}
                override = deepcopy(metadata)
                override.update(ExceptionHandling='Sync', FloatingPointModel='Strict')
                configs[config] = {'properties': {}, 'definitions': {'ClCompile': metadata},
                                   'items': [{'kind': 'ClCompile', 'path': str(root / 'override.cpp'),
                                              'metadata': override,
                                              'overrides': {'ExceptionHandling': 'Sync', 'FloatingPointModel': 'Strict'}}]}
            lines = []
            translator.emit_portable_game_policy(lines, 'fixture', configs, root / 'fixture.vcxproj')
            (root / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                'project(Policy CXX)\nadd_library(fixture STATIC ordinary.cpp override.cpp)\n' + '\n'.join(lines) + '\n')
            for config, diagnostics in [('Release', 'OFF'), ('Release', 'ON'), ('Debug', 'OFF')]:
                build = root / (config + diagnostics)
                result = subprocess.run(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', f'-DCMAKE_BUILD_TYPE={config}',
                    f'-DCSE_RELEASE_DIAGNOSTICS={diagnostics}', '-DCMAKE_CXX_FLAGS_RELEASE='], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                commands = json.loads((build / 'compile_commands.json').read_text())
                for command in commands:
                    options = shlex.split(command['command'])
                    exceptions = [option for option in options if option in ('-fexceptions', '-fno-exceptions')]
                    floating = [option for option in options if option.startswith('-ffp-model=')]
                    overridden = Path(command['file']).name == 'override.cpp'
                    self.assertEqual(exceptions[-1], '-fexceptions' if overridden else '-fno-exceptions')
                    self.assertEqual(floating[-1], '-ffp-model=strict' if overridden else '-ffp-model=precise')
                    self.assertIn('-ffp-contract=off', options)
                    self.assertIn('-DUTL_ENGINE_ASSERTS', options)
                    self.assertNotIn('-DWIN32', options)
                    self.assertNotIn('-DNDEBUG', options)
                    self.assertEqual('-g' in options, config == 'Debug' or diagnostics == 'ON')


if __name__ == '__main__':
    unittest.main()
