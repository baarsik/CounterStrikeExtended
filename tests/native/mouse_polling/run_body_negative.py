"""Reject camera handoff defects in complete actual client function bodies."""
from pathlib import Path
import argparse
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sdl', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
args.output.mkdir(parents=True, exist_ok=False)
results = []
for game in ['hl', 'cs']:
    relative = 'src/' + ('cstrike/' if game == 'cs' else '') + 'cl_dll/inputw32.cpp'
    data = (root / relative).read_bytes()
    start = data.index(b'void IN_SetCameraMouseOwnership(bool owned) {')
    end = data.index(b'\n}\n#endif', start) + 2
    body = data[start:end]
    variants = {
        'noop': b'void IN_SetCameraMouseOwnership(bool owned) { (void)owned; }',
        'keep-fraction': body.replace(b'    s_relativeMotion.Reset();', b'    (void)0;'),
        'reset-filter': body.replace(b'    mx_accum = 0;', b'    old_mouse_x = 0;\n    mx_accum = 0;'),
        'pump-events': body.replace(b'    SDL_GetRelativeMouseState(', b'    SDL_PumpEvents();\n    SDL_GetRelativeMouseState('),
        'non-idempotent': body.replace(b'    if (s_cameraMouseOwned == owned) {\n        return;\n    }', b''),
    }
    for name, changed in variants.items():
        assert changed != body
        case = args.output / (game + '-' + name)
        source = case / 'source'
        for other, path in [('hl', 'src/cl_dll/inputw32.cpp'),
                            ('cs', 'src/cstrike/cl_dll/inputw32.cpp')]:
            target = source / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data[:start] + changed + data[end:] if other == game else (root / path).read_bytes())
        build = case / 'build'
        commands = [['cmake', '-S', str(root / 'tests/native/client_input'), '-B', str(build), '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=Release', '-DCSE_INPUT_SOURCE_ROOT=' + str(source),
                     '-DCSE_SDL_INCLUDE=' + str(args.sdl)],
                    ['cmake', '--build', str(build), '--target', game + '_input_tests', '--parallel', '2']]
        for index, command in enumerate(commands):
            run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (case / ('stage-' + str(index) + '.log')).write_bytes(run.stdout)
            assert run.returncode == 0, (name, run.stdout)
        run = subprocess.run([str(build / (game + '_input_tests'))], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=10)
        (case / 'run.log').write_bytes(run.stdout)
        assert run.returncode != 0 and b'check:' in run.stdout, (name, run.stdout)
        results.append({'game': game, 'mutant': name, 'exit': run.returncode,
                        'output': run.stdout.decode(errors='replace')})
(args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('rejected actual-body handoff mutants:', len(results))
