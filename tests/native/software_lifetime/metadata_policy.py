"""Check the authoritative CMake selection and per-source software lifetime policy."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target

TERMINAL = str(ROOT / 'src/engine/native_software_lifetime.cpp')
CLIPBOARD = str(ROOT / 'src/common/TextEntryClipboard.cpp')
reference = {}
for configuration in ('Debug', 'Release'):
    for target_os, architecture in (('macos', 'arm64'), ('macos', 'x64'),
                                    ('windows', 'x86'), ('windows', 'x64'), ('linux', 'x64')):
        data = query_target(ROOT, 'swds', configuration, target_os, architecture)
        selected = {item['path']: item for item in data['sources'] if item['language']}
        expected = target_os == 'macos'
        assert (TERMINAL in selected) == expected, (configuration, target_os, architecture)
        for source in (TERMINAL, CLIPBOARD):
            enabled = source in selected and 'CSE_TEXT_ENTRY_USE_PTHREAD_KEY=1' in selected[source]['compile_definitions']
            assert enabled == expected, (source, configuration, target_os, architecture)
        if expected:
            options = selected[TERMINAL]['compile_options']
            exceptions = [flag for flag in options if flag in ('-fexceptions', '-fno-exceptions')]
            assert exceptions[-1] == '-fexceptions', (configuration, options)
            assert '-ffp-model=precise' in options
            assert '-ffp-contract=off' in options
        if target_os == 'windows' and architecture == 'x86':
            reference[configuration] = {
                'sources': [{key: item[key] for key in ('path', 'language', 'compile_options',
                                                        'include_directories', 'compile_definitions')}
                            for item in data['sources'] if item['language']],
                'runtime': data['runtime'],
            }
    hardware = query_target(ROOT, 'hw_engine', configuration, 'macos', 'arm64')
    assert not any(item['path'] == TERMINAL and item['language'] for item in hardware['sources'])
if len(sys.argv) > 1:
    before = json.loads(Path(sys.argv[1]).read_text())
    assert reference == before, 'Reference Win32 CMake compiler recipe changed'
print('PASS Apple terminal/clipboard ownership, exception policy and retained Win32 CMake recipes')
