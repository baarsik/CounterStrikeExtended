"""Require runtime rejection of private helper variants; production stays unchanged."""
from pathlib import Path
import json
import shutil
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
sdl = Path(sys.argv[3]).resolve()
header = (root / 'src/engine/sdl3_window.h').read_bytes()
commit = b'    *x = resultX;\n    *y = resultY;\n    *centerX = resultCenterX;\n    *centerY = resultCenterY;'
combined_axes = b'''    if (!tryAxis(windowX, space.windowWidth, space.renderWidth, space.aspectX, resultCenterX, resultX) ||
        !tryAxis(windowY, space.windowHeight, space.renderHeight, space.aspectY, resultCenterY, resultY)) {
        return false;
    }'''
separate_axes = b'''    if (!tryAxis(windowX, space.windowWidth, space.renderWidth, space.aspectX, resultCenterX, resultX)) {
        return false;
    }
    *x = resultX;
    if (!tryAxis(windowY, space.windowHeight, space.renderHeight, space.aspectY, resultCenterY, resultY)) {
        return false;
    }'''
variants = [
    ('no-op', commit, b'    (void)resultX;\n    (void)resultY;\n    (void)resultCenterX;\n    (void)resultCenterY;',
     'output->x == 1000'),
    ('desktop-center', b'    const int resultCenterX = space.renderWidth / 2;\n'
     b'    const int resultCenterY = space.renderHeight / 2;', b'''    int desktopX = 0;
    int desktopY = 0;
    SDL_GetWindowPosition(window, &desktopX, &desktopY);
    const int resultCenterX = desktopX + space.renderWidth / 2;
    const int resultCenterY = desktopY + space.renderHeight / 2;''', 'output->x == 1000'),
    ('double-read', b'    float windowX = 0.0f;',
     b'    SDL_GetWindowSize(window, &space.windowWidth, &space.windowHeight);\n    float windowX = 0.0f;',
     'controlled.sizeReads == 1'),
    ('partial-write', combined_axes, separate_axes, 'output.x == 101'),
    ('camera-difference-overflow', b'''        if (adjustedDifference < minimumPosition || adjustedDifference > maximumPosition) {
            return false;
        }''', b'        (void)adjustedDifference;', '!succeeded'),
    ('relative-drain', b'    MouseCoordinateSpace space;\n    if (!SDL_GetWindowSize',
     b'    SDL_GetRelativeMouseState(nullptr, nullptr);\n    MouseCoordinateSpace space;\n    if (!SDL_GetWindowSize',
     'controlled.forbiddenCalls == 0'),
    ('double-scaling', b'        const float scaledValue = position * static_cast<float>(renderExtent) / '
     b'static_cast<float>(windowExtent);',
     b'        const double scaledValue = static_cast<double>(position) * renderExtent / windowExtent;',
     'output.x == cse::WindowToRenderCoordinate'),
    ('overstrict-truncation', b'adjustedValue <= truncationLowerExclusive',
     b'adjustedValue < static_cast<double>(minimumPosition)', 'Query(negativeFraction)'),
]
start = header.index(b'// Main-thread query only.')
prefix = header[:start]
helper = header[start:]
results = []
for name, original, changed, expected in variants:
    assert helper.count(original) == 1, name
    case = output / name
    source = case / 'source/src'
    path = source / 'engine/sdl3_window.h'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(prefix + helper.replace(original, changed))
    for relative in ['common/sdl3_input.h', 'common/sdl3_policy.h']:
        target = source / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / 'src' / relative, target)
    build = case / 'build'
    commands = [
        ['cmake', '-S', str(root / 'tests/native/client_mouse_coordinates'), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DCSE_MOUSE_WINDOW_HEADER=' + str(path), '-DCSE_SDL_INCLUDE=' + str(sdl)],
        ['cmake', '--build', str(build), '--parallel', '2'],
    ]
    for index, command in enumerate(commands):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (case / ('stage-' + str(index) + '.log')).write_bytes(result.stdout)
        assert result.returncode == 0, (name, result.stdout.decode(errors='replace'))
    result = subprocess.run([str(build / 'client_mouse_coordinates_tests')],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (case / 'run.log').write_bytes(result.stdout)
    assert result.returncode != 0 and expected.encode() in result.stdout, (name, result.stdout)
    results.append({'variant': name, 'exit': result.returncode, 'expected': expected,
                    'output': result.stdout.decode()})
    print(name, 'rejected at', expected, flush=True)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
assert (root / 'src/engine/sdl3_window.h').read_bytes() == header
