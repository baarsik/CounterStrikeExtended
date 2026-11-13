#!/usr/bin/env python3
"""Measure shipped live labels with fresh actual VGUI1 Font/bitmap/native providers."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True, help='Actual staged runtime containing valve font TGAs/schemes')
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--provider-build', type=Path, default=Path('build/vgui1-utf8'))
a = p.parse_args()
r = Path(__file__).resolve().parents[3]
out = a.evidence.resolve(); out.mkdir(parents=True, exist_ok=True)
runtime = a.runtime.resolve()
source = (r / 'src/cl_dll/vgui_ServerBrowser.cpp').read_text(encoding='cp1252')
assert 'int bw = 80, bh = 15;' in source
assert 'void ServerBrowser::LayoutPresentation()' in source
assert '#define CSIZE_ADDRESS' in source and 'XRES(200)' in source
records = []
manifest = {}
languages = ['english', 'russian', 'polish', 'german', 'french', 'italian', 'spanish', 'schinese', 'tchinese', 'koreana']
for language in languages:
    titles = r / ('src/deploy/localization/valve/titles.txt' if language == 'english' else
                  f'src/deploy/localization/valve_{language}/titles.txt')
    values = dict(re.findall(r'(?m)^([A-Za-z0-9_]+)[ \t]*\n\{\n(.*?)\n\}', titles.read_text(), re.S))
    manifest[str(titles)] = hashlib.sha256(titles.read_bytes()).hexdigest()
    for resolution, screen_width, screen_height in [(640, 640, 480), (1024, 1024, 768), (1280, 1280, 720)]:
        scheme = runtime / 'valve' / f'{resolution}_textscheme.txt'
        settings = dict((name, (font, int(size))) for name, font, size in
            re.findall(r'SchemeName\s*=\s*"([^\"]+)"\s*FontName\s*=\s*"([^\"]+)"\s*FontSize\s*=\s*(\d+)', scheme.read_text()))
        manifest[str(scheme)] = hashlib.sha256(scheme.read_bytes()).hexdigest()
        def add(token, context, role, width, height, prefix=''):
            _, pixels = settings[role]
            bitmap = runtime / 'valve/gfx/vgui/fonts' / f'{resolution}_{role}.tga'
            manifest[str(bitmap)] = hashlib.sha256(bitmap.read_bytes()).hexdigest()
            records.append([language, f'{screen_width}x{screen_height}', context, token, str(bitmap), str(pixels),
                            str(width), str(height), prefix + values[token]])
        for token in ['Connect', 'Refresh', 'Stop', 'Close']:
            for width in [80, 75]:
                add('CSE_UI_' + token, f'browser-button-{width}px', 'Primary Button Text', width, 15, '     ')
        for token, width in [('Address', 200), ('Server', 200), ('Map', 100), ('Current', 70), ('Latency', 70)]:
            add('CSE_UI_' + token, 'browser-column', 'Scoreboard Text', width * screen_width // 640,
                18 * screen_height // 480)
        for token in ['Waiting', 'Searching']:
            add('CSE_UI_' + token, 'browser-first-cell-status', 'Scoreboard Text', 200 * screen_width // 640 - 2,
                15 * screen_height // 480 - 2)
        # These labels have no active retained control: LAN/Test/Sort are inside
        # disabled source blocks and CSE_UI_Players currently has no source use.
records_file = out / 'records.bin'
records_file.write_bytes(b''.join(field.encode() + b'\0' for record in records for field in record))
manifest[str(r / 'src/cl_dll/vgui_ServerBrowser.cpp')] = hashlib.sha256((r / "src/cl_dll/vgui_ServerBrowser.cpp").read_bytes()).hexdigest()
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2))
subprocess.run(['python3', str(r / 'tests/native/vgui1_cjk/run.py'), '--provider-build', str(a.provider_build.resolve()),
                '--evidence', str(out), '--bitmap', str(records_file), '--driver',
                str(r / 'tests/native/hl_menu_localization/live_label_fit.cpp')], check=True)
raw = (out / 'run.log').read_text()
(out / 'measurements.tsv').write_text(raw)
lines = raw.splitlines(); overflow = [line.split('\t') for line in lines[1:] if '\toverflow\t' in line]
(out / 'summary.json').write_text(json.dumps({'records': len(records), 'provider_measurements': len(lines) - 1,
    'overflows': len(overflow), 'russian_polish': [row for row in overflow if row[0] in ('russian', 'polish')]},
    ensure_ascii=False, indent=2))
print(f'{len(records)} labels/bounds; {len(lines)-1} actual provider measurements; {len(overflow)} overflows')
for row in overflow:
    if row[0] in ('russian', 'polish') and row[1] == '640x480': print('\t'.join(row))

subprocess.run(['python3', str(r / 'tests/native/hl_menu_localization/live_label_layout.py'),
                '--metrics', str(out / 'measurements.tsv'), '--evidence', str(out / 'adaptive-layout')], check=True)
