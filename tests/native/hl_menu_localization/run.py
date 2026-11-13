#!/usr/bin/env python3
"""Compile actual HL title expansion/menu scratch/button text owners under sanitizers."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--case', choices=('menu', 'bounds', 'all'), default='all')
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
paths = ['src/cl_dll/text_message.cpp', 'src/cl_dll/vgui_TeamFortressViewport.cpp',
         'src/cl_dll/vgui_TeamFortressViewport.h', 'src/cl_dll/vgui_CustomObjects.cpp']
text, viewport, header, buttons = [(root / name).read_text() for name in paths]
helper = []
if 'LocalizedPrefixBytes(' in text:
    helper.append(extract.extract(text, 'LocalizedPrefixBytes'))
localization_start = text.rfind('\n', 0, text.index('CHudTextMessage::LocaliseTextString(')) + 1
localization_end = text.index('\n}', localization_start) + 2
helper.append(text[localization_start:localization_end])
(out / 'localize.inc').write_text('\n\n'.join(helper) + '\n')
menu = extract.extract(viewport, 'TeamFortressViewport::CreateCommandMenu')
constants = re.findall(r'^\s*constexpr int CommandMenuLabelBytes[^;]*;', menu, re.M)
last = re.search(r'char szLastButtonText\[[^\]]+\][^;]*;', menu)[0]
label = re.search(r'char cText\[[^\]]+\][^;]*;', menu)[0]
copy = re.search(r'CHudTextMessage::LocaliseTextString\(\s*token, cText[^;]+;', menu)[0]
last_copy = re.search(r'strcpy\(\s*szLastButtonText, cText\s*\);', menu)[0]
(out / 'menu.inc').write_text('MenuResult ActualMenuLabel(const char* token) {\n' + '\n'.join(constants) +
    '\n' + last + '\n' + label + '\n' + copy + '\n' + last_copy +
    '\nreturn {cText, szLastButtonText};\n}\n')
field = re.search(r'(?:char m_sMainText\[[^\]]+\]|std::string m_sMainText)\s*;', header)[0]
(out / 'button-field.inc').write_text(field + '\n')
(out / 'button.inc').write_text('\n\n'.join(extract.extract(buttons, name) for name in
    ('CommandButton::RecalculateText', 'CommandButton::setText', 'CommandButton::setBoundKey')) + '\n')
command = ['c++', '-std=c++17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
           '-Wno-deprecated-declarations', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I', str(out),
           '-I', str(root / 'src/common'), str(root / 'tests/native/hl_menu_localization/menu_tests.cpp'),
           '-o', str(out / 'menu_tests')]
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
subprocess.run(command, check=True)
results = []
for language in ('russian', 'polish'):
    titles = root / f'src/deploy/localization/valve_{language}/titles.txt'
    records = dict(re.findall(r'(?m)^([A-Za-z0-9_]+)[ \t]*\n\{\n(.*?)\n\}', titles.read_text(), re.S))
    menus = [root / f'src/deploy/localization/valve_{language}/{name}.txt'
             for name in ('spectatormenu', 'spectcammenu')]
    tokens = set()
    for menu in menus:
        tokens.update(re.findall(r'"#([A-Za-z0-9_]+)"', menu.read_text()))
    assert tokens and tokens.issubset(records), tokens - records.keys()
    data = out / f'{language}.titles'
    data.write_bytes(b''.join(name.encode() + b'\0' + records[name].encode() + b'\0' for name in sorted(tokens)))
    paths.extend(str(path.relative_to(root)) for path in [titles] + menus)
    result = subprocess.run([str(out / 'menu_tests'), str(data), args.case], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / f'{language}.log').write_text(result.stdout)
    print(language, result.returncode, result.stdout)
    results.append({'language': language, 'labels': len(tokens), 'exit_code': result.returncode})
(out / 'source-manifest.json').write_text(json.dumps({name: hashlib.sha256((root / name).read_bytes()).hexdigest()
    for name in paths}, indent=2) + '\n')
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
if any(result['exit_code'] != 0 for result in results):
    raise SystemExit(1)
