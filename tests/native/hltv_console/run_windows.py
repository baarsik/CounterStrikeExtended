#!/usr/bin/env python3
"""Compile frozen before/after actual-base history fixtures in an existing MSVC environment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--arch', choices=['x86', 'x64', 'arm64'], required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    compiler = Path(shutil.which('cl.exe')).resolve()
    receipt = {'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
               'arch': options.arch, 'compiles': [], 'cases': [], 'qualification': 'actual base; controlled allocation/output bindings; no backend/provider/game'}
    for phase in ['before', 'after']:
        destination = options.output / phase
        shutil.copytree(options.source_stage / phase, destination, dirs_exist_ok=True)
        shutil.copy2(options.source_stage / 'history_tests.cpp', destination / 'history_tests.cpp')
        sdk = destination / 'GoldSrc-SDK'
        includes = ['HLTV/Core/private', 'HLTV/Core/private/common', 'HLTV/Console', 'HLTV/common',
                    'engine', 'vgui2/src/public', 'public', 'common', '.']
        common = ['/nologo', '/std:c++14', '/W4', '/WX', '/EHsc', '/GR', '/fp:precise', '/Gy',
                  '/DWIN32', '/D_WINDOWS', '/DHLTV_CONSOLE', '/DCLIENT_DLL', '/DNOMINMAX',
                  '/DUTL_ENGINE_ASSERTS', '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_WARNINGS']
        common += ['/I' + str(sdk / value) for value in includes]
        sources = [sdk / 'HLTV/Console/TextConsole.cpp', sdk / 'common/ObjectList.cpp', destination / 'history_tests.cpp']
        for configuration, flags in [('release', ['/O2', '/MD']), ('debug', ['/Od', '/RTC1', '/Zi', '/MDd', '/D_DEBUG'])]:
            build = destination / configuration
            build.mkdir(exist_ok=True)
            binary = build / 'history.exe'
            command = [str(compiler), *common, *flags, *map(str, sources), '/Fe:' + str(binary), '/link', '/WX']
            result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
            (build / 'compile.log').write_bytes(result.stdout + result.stderr)
            receipt['compiles'].append({'phase': phase, 'configuration': configuration, 'args': command,
                                        'cwd': str(build), 'exit': result.returncode})
            if result.returncode:
                (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
                raise RuntimeError('actual-base compile failed: ' + str(build / 'compile.log'))
            for entries in [29, 30, 31, 59, 60]:
                run = subprocess.run([str(binary), str(entries)], cwd=build, capture_output=True, timeout=10)
                (build / ('history-' + str(entries) + '.log')).write_bytes(run.stdout + run.stderr)
                expected_success = phase == 'after' or entries == 29
                matched = (run.returncode == 0) == expected_success
                receipt['cases'].append({'phase': phase, 'configuration': configuration, 'entries': entries,
                                         'exit': run.returncode, 'expected_success': expected_success, 'matched': matched})
                if not matched:
                    (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
                    raise RuntimeError('unexpected actual-base history result')
    (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
    print('PASS', options.arch, len(receipt['compiles']), 'actual-base builds;', len(receipt['cases']), 'before/after outcomes')


if __name__ == '__main__':
    main()
