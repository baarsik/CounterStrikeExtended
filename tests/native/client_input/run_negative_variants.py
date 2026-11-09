"""Run rejected behavior changes only on private copies of the production bodies."""
from pathlib import Path
import json
import shutil
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
sdl = Path(sys.argv[3]).resolve()
output.mkdir(parents=True, exist_ok=True)
variants = [
    ('cs-init-nonzero', 'cs', 'CVAR_GET_FLOAT( "m_rawinput" ) > 0;',
     'CVAR_GET_FLOAT( "m_rawinput" ) != 0;', '!m_bRawInput && services.rawReads == 1'),
    ('cs-refresh-at-one', 'cs', 'gpGlobals->time - s_flRawInputUpdateTime > 1.0f',
     'gpGlobals->time - s_flRawInputUpdateTime >= 1.0f', '!m_bRawInput && services.rawReads == 1'),
    ('cs-refresh-positive', 'cs', 'm_bRawInput = CVAR_GET_FLOAT( "m_rawinput" ) != 0;',
     'm_bRawInput = CVAR_GET_FLOAT( "m_rawinput" ) > 0;', 'm_bRawInput && services.rawReads == 2'),
    ('hl-filter-current-only', 'hl', 'mouse_x = (mx + old_mouse_x) * 0.5;',
     'mouse_x = mx;', 'mouse_x == 2.0f && mouse_y == 4.0f'),
    ('hl-no-raw-transition-reset', 'hl', 's_relativeMotion.SetRawInput(IN_UseRawInput());',
     '(void)0;', 'old_mouse_x == 0'),
    ('cs-no-zoom', 'cs', 'if ( gHUD.m_iFOV != gHUD.default_fov->value )',
     'if ( false )', 'x == 8.25f && y == 11.0f'),
]
results = []
for name, game, original, replacement, expected in variants:
    case = output / name
    source = case / 'source'
    for candidate, relative in [('hl', 'GoldSrc-SDK/cl_dll/inputw32.cpp'),
                                ('cs', 'GoldSrc-SDK/cstrike/cl_dll/inputw32.cpp')]:
        path = root / relative
        data = path.read_bytes()
        if candidate == game:
            count = data.count(original.encode())
            assert count == (2 if name == 'hl-no-raw-transition-reset' else 1), (name, count)
            data = data.replace(original.encode(), replacement.encode())
        target = source / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    build = case / 'build'
    commands = [
        ['cmake', '-S', str(root / 'tests/native/client_input'), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DCSE_INPUT_SOURCE_ROOT=' + str(source), '-DCSE_SDL_INCLUDE=' + str(sdl)],
        ['cmake', '--build', str(build), '--target', game + '_input_tests', '--parallel', '2'],
    ]
    for index, command in enumerate(commands):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (case / ('stage-' + str(index) + '.log')).write_bytes(result.stdout)
        assert result.returncode == 0, (name, result.stdout.decode(errors='replace'))
    result = subprocess.run([str(build / (game + '_input_tests'))], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (case / 'run.log').write_bytes(result.stdout)
    assert result.returncode != 0 and expected.encode() in result.stdout, (name, result.stdout)
    results.append({'variant': name, 'game': game, 'exit': result.returncode,
                    'expected_failure': expected, 'output': result.stdout.decode()})
    print(name, 'rejected at', expected, flush=True)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
