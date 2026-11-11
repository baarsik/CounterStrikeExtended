#!/usr/bin/env python3
"""Actual Console owner with controlled VGUI services; no live UI is operated."""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

from run import check_cleanup, compile_fixture, digest, require, run


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-build', type=Path, required=True)
    parser.add_argument('--filesystem', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize-console', action='store_true')
    options = parser.parse_args()
    build = options.runtime_build.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='vgui-', dir=options.output.resolve()))
    print('STAGE', stage, flush=True)
    (stage / 'valve').mkdir()
    profile = re.search(r'CMAKE_BUILD_TYPE:STRING=(.*)', (build / 'CMakeCache.txt').read_text())[1]
    runtime = build / 'runtime' / profile
    for name in ('Core.dylib', 'proxy.dylib', 'libsteam_api.dylib'):
        shutil.copy2(runtime / name, stage / name)
    shutil.copy2(options.filesystem, stage / 'filesystem_stdio.dylib')
    directory = Path(__file__).resolve().parent
    compile_fixture(['c++', '-std=c++17', '-dynamiclib', str(directory / 'vgui_services/provider.cpp'),
                     '-o', str(stage / 'vgui2.dylib')], build)
    entry = next(item for item in json.loads((build / 'compile_commands.json').read_text())
                 if item['file'].endswith('/HLTV/Console/System.cpp'))
    args = shlex.split(entry['command'])
    args[1:1] = ['-DCONTROLLED_VGUI_SERVICES', '-I' + str(directory / 'vgui_services')]
    args[args.index('-c') + 1] = str(directory / 'console_paths.cpp')
    obj = stage / 'console_vgui.o'
    args[args.index('-o') + 1] = str(obj)
    sanitizer = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if options.sanitize_console else []
    compile_fixture(args + sanitizer, build)
    command = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'hltv'], text=True).splitlines()[-1]
    link = shlex.split(command.split('&&')[1])
    # Link real Console/common/platform ownership services. Controlled VGUI services
    # replace the unrelated GUI control implementations, not any System method.
    objects = {'TextConsole.cpp.o', 'common.cpp.o', 'random.cpp.o', 'ObjectList.cpp.o', 'TokenLine.cpp.o',
               'interface.cpp.o', 'TextConsolePosix.cpp.o', 'PosixSystem.cpp.o',
               'legacy_wide_format.cpp.o', 'BaseSystemModule.cpp.o'}
    link = [argument for argument in link if not argument.endswith('.o') or Path(argument).name in objects]
    link.insert(1, str(obj))
    link[link.index('-o') + 1] = str(stage / 'console_vgui_paths')
    compile_fixture(link + sanitizer, build)
    for path in stage.iterdir():
        if path.is_file():
            print('SHA256', digest(path), path, flush=True)
    failures = []
    for case in ('vgui-failure', 'vgui-success', 'vgui-retry'):
        try:
            output = run(stage, 'console_vgui_paths', [case])
            check_cleanup(output, expected=2)
            events = [line for line in output.splitlines() if line.startswith('VGUI ')]
            acquired = events.count('VGUI panel-acquired panels=1 embedded=0')
            expected = 2 if case == 'vgui-retry' else 1
            require(acquired == expected, f'{case}: acquired {acquired} panels, expected {expected}')
            require(events.count('VGUI panel-destroyed panels=0 embedded=0') == expected,
                    f'{case}: missing destruction after detach')
            require(events.count('VGUI surface-shutdown panels=0 embedded=0') == expected,
                    f'{case}: surface shutdown must follow panel destruction')
            require(events.count('VGUI provider-unload panels=0 embedded=0') == expected,
                    f'{case}: owned panels must retire before every provider unload')
            require(events.count('VGUI controls-stop panels=1 embedded=1') == (case != 'vgui-failure'),
                    f'{case}: started controls must stop exactly once')
            last_acquired = max(index for index, event in enumerate(events) if 'panel-acquired' in event)
            deferred = events.index('VGUI owner-deferred')
            if case != 'vgui-failure':
                require(last_acquired < deferred < len(events) - 1,
                        f'{case}: final VGUI resources must survive deferred owner retirement')
        except AssertionError as error:
            failures.append(str(error))
    print('FAILURES', failures, flush=True)
    require(not failures, '; '.join(failures))
    print('PASS 3 actual-owner controlled-VGUI lifecycle cases')


if __name__ == '__main__':
    main()
