"""Extract the retained Win32 methods verbatim; no production or registry writes."""
import argparse
import hashlib
import json
from pathlib import Path


def function(text, start):
    begin = text.index(start)
    brace = text.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    base = args.root / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'
    app = (base / 'win32/App.cpp').read_text()
    common = (base / 'vgui.cpp').read_text()
    definitions = 'namespace vgui {\n' + function(common, 'void vgui_strcpy(') + '\n}\n'
    definitions += 'namespace {\n' + function(app, 'bool SplitRegistryName(') + '\n}\n'
    definitions += 'namespace vgui {\n' + '\n\n'.join(function(app, 'bool App::' + name + '(')
        for name in ['setRegistryString', 'getRegistryString', 'setRegistryInteger', 'getRegistryInteger']) + '\n}\n'
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'app_registry.inc').write_text(definitions)
    inputs = {str(path.relative_to(args.root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [base / 'win32/App.cpp', base / 'vgui.cpp']}
    (args.output / 'extracted-inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')


if __name__ == '__main__':
    main()
