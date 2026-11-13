#!/usr/bin/env python3
"""Compile complete selected provider methods with controlled Win32 OS services."""
import argparse
import hashlib
from pathlib import Path
import shlex
import subprocess


def extract(path, signature):
    source = path.read_text(encoding='cp1252')
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=['release', 'debug', 'sanitize'], required=True)
    options = parser.parse_args()
    directory = Path(__file__).resolve().parent
    root = directory.parents[3]
    sdk = root / 'src'
    options.output.mkdir(parents=True, exist_ok=True)
    output = options.output.resolve()
    methods = []
    for signature in ('void CWin32Surface::SetEmbeddedPanel(', 'void CWin32Surface::SetContextPanel(',
                      'SurfacePlat* CWin32Surface::ContextPlat(', 'void CWin32Surface::CreatePopup(',
                      'void CWin32Surface::ReleasePanel(', 'void CWin32Surface::SetPanelVisible('):
        methods.append(extract(sdk / 'vgui2/src/Surface.cpp', signature))
    for signature in ('bool VPanel::IsPopup(', 'SurfacePlat* VPanel::Plat(', 'void VPanel::SetPlat(',
                      'VPanel* VPanel::GetParent('):
        methods.append(extract(sdk / 'vgui2/src/VPanel.cpp', signature))
    generated = output / 'provider_methods.cpp'
    generated.write_text('#include "fixture.h"\n' + '\n\n'.join(methods) + '\n')
    flags = ['-O2'] if options.profile == 'release' else ['-O0', '-g']
    if options.profile == 'sanitize':
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    command = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(directory),
               str(generated), str(directory / 'paths.cpp'), '-o', str(output / 'surface_provider'), *flags]
    print('BUILD', shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    for path in (sdk / 'vgui2/src/Surface.cpp', sdk / 'vgui2/src/VPanel.cpp', generated, output / 'surface_provider'):
        print('SHA256', hashlib.sha256(path.read_bytes()).hexdigest(), path, flush=True)
    subprocess.run([str(output / 'surface_provider')], check=True)


if __name__ == '__main__':
    main()
