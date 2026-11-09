from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--sdl-include', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--sdl-config', type=Path)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
core = args.repo / 'GoldSrc-SDK/engine/native_mouse_window.cpp'
cocoa = args.repo / 'GoldSrc-SDK/common/sdl3_native_cocoa_mouse.mm'
windows = args.repo / 'GoldSrc-SDK/engine/native_mouse_windows.cpp'
cases = [
 ('absolute-cache', core, 'CORE', 'native_mouse_window_tests',
  '} else if (!platform.absolutePosition(handle, width, height, &resultX, &resultY)) {\n        return false;\n    }',
  '} else {\n        SDL_GetMouseState(&resultX, &resultY);\n    }'),
 ('relative-consume', core, 'CORE', 'native_mouse_window_tests',
  '        SDL_GetMouseState(&resultX, &resultY);', '        SDL_GetRelativeMouseState(&resultX, &resultY);'),
 ('no-keyboard-owner', core, 'CORE', 'native_mouse_window_tests',
  ' || SDL_GetKeyboardFocus() != target', ''),
 ('no-native-binding', core, 'CORE', 'native_mouse_window_tests',
  'SDL_GetWindowFromID(id) != target ||\n        SDL_GetPointerProperty(props, platform.windowProperty, nullptr) != handle',
  'SDL_GetWindowFromID(id) != target'),
 ('stale-ticket', core, 'CORE', 'native_mouse_window_tests',
  ' && version == transaction.version', ''),
 ('lost-retry', core, 'CORE', 'native_mouse_window_tests',
  '    return ready && pending && pendingOwner == owner;', '    (void)owner;\n    return false;'),
 ('trust-startup-return', core, 'CORE', 'native_mouse_window_tests',
  'CseSdl_QueryMouseModeV1(CSE_SDL_MOUSE_MODE_CONTRACT_V1) == CseSdlMouseModeAbsolute &&', 'true &&'),
 ('partial-output', core, 'CORE', 'native_mouse_window_tests',
  '    // The provider rejects a foreign/wrong-thread query before any owner/video access.',
  '    *x = 0.0f;\n    // Deliberate mutation before admission.'),
]
if os.name == 'nt':
 cases += [
 ('unchecked-cursor', windows, 'WINDOWS', 'native_mouse_platform_tests', '!s.cursor(&point) || ', '(s.cursor(&point), false) || '),
 ('no-client-conversion', windows, 'WINDOWS', 'native_mouse_platform_tests', '!s.screenToClient(window, &point) || ', ''),
 ('no-mirror-guard', windows, 'WINDOWS', 'native_mouse_platform_tests', ' || (style & WS_EX_LAYOUTRTL) != 0', ''),
 ('no-dpi-guard', windows, 'WINDOWS', 'native_mouse_platform_tests', '!s.dpiEqual(context, windowContext) || ', ''),
 ]
else:
 cases += [
 ('wrong-flip', cocoa, 'COCOA', 'native_mouse_platform_tests', 's.flipped(view) ? relativeY : bounds.size.height - relativeY', 'bounds.size.height - relativeY'),
 ('wrong-origin', cocoa, 'COCOA', 'native_mouse_platform_tests', 'point.x - bounds.origin.x', 'point.x'),
 ('wrong-extent', cocoa, 'COCOA', 'native_mouse_platform_tests', 'rect.size.width != static_cast<double>(width) ||', 'false ||'),
 ('wrong-mode-admission', cocoa, 'COCOA', 'native_mouse_platform_tests',
  'requiredMode != AnyMouseMode && mode != requiredMode', 'requiredMode != AnyMouseMode && false'),
 ('foreign-status-erased', cocoa, 'COCOA', 'native_mouse_platform_tests',
  'return CseSdlCocoaMouseForeignDispatch;', 'return CseSdlCocoaMouseNotReady;'),
 ('thread-status-erased', cocoa, 'COCOA', 'native_mouse_platform_tests',
  'case CseSdlMouseModeWrongThread:\n        return CseSdlCocoaMouseWrongThread;',
  'case CseSdlMouseModeWrongThread:\n        return CseSdlCocoaMouseRejected;'),
 ]
records = []
for name, source, variable, target, before, after in cases:
    text = source.read_text()
    assert text.count(before) == 1, (name, text.count(before))
    mutant = args.evidence / (name + source.suffix)
    mutant.write_text(text.replace(before, after))
    build = args.build / name
    configure = ['cmake', '-S', args.repo / 'tests/native/native_mouse_window', '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Debug', '-DCSE_SDL_INCLUDE=' + str(args.sdl_include),
        '-DCSE_NATIVE_MOUSE_' + variable + '_SOURCE=' + str(mutant)]
    if os.name != 'nt':
        configure.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0')
        if args.sdl_config:
            configure.append('-DCSE_SDL_CONFIG=' + str(args.sdl_config))
    commands = [('configure', configure, 0), ('build', ['cmake', '--build', build, '--target', target, '--parallel', '2'], 0),
        ('run', [build / (target + ('.exe' if os.name == 'nt' else ''))], 17)]
    for phase, argv, expected in commands:
        result = subprocess.run([str(x) for x in argv], capture_output=True, text=True, timeout=180)
        (args.evidence / (name + '-' + phase + '.log')).write_text(result.stdout + result.stderr)
        records.append(dict(name=name, phase=phase, argv=[str(x) for x in argv], exit=result.returncode,
            expected=expected, original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            mutant_sha256=hashlib.sha256(mutant.read_bytes()).hexdigest()))
        (args.evidence / 'receipt.json').write_text(json.dumps(records, indent=2) + '\n')
        assert result.returncode == expected, records[-1]
    print(name + ':build0/run17', flush=True)
