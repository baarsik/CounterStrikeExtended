#!/usr/bin/env python3
"""Emit real control settings for the native font/layout fixture."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
rows = json.loads(Path(__file__).with_name('fit_controls.json').read_text())['controls']

def quote(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'

lines = ['"TranslationFit"', '{']
for index, row in enumerate(rows):
    settings = row['settings'].copy()
    settings.update({'source': row['resource'], 'category': row['category'], 'control': row['control'],
                     'kind': row['kind'], 'xpos': '16', 'ypos': '64', 'visible': '1'})
    lines += [quote(index), '{']
    lines += [quote(key) + ' ' + quote(value) for key, value in settings.items()]
    lines += ['}']
lines += ['}']
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n'.join(lines) + '\n')
print('Prepared', len(rows), 'actual resource control constraints')
