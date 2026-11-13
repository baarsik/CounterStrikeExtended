"""Measure both ZBot layouts with actual controls/fonts in every bundled language."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--component', type=Path, required=True)
parser.add_argument('--game', type=Path, required=True, help='Read-only Half-Life game root')
parser.add_argument('--runtime', type=Path, default=ROOT / 'build/runtime-macos-arm64-release/runtime/Release')
parser.add_argument('--output', type=Path, default=ROOT / 'build/zbot-layout/languages')
args = parser.parse_args()
component = args.component.resolve()
runtime = args.runtime.resolve()
game = args.game.resolve()
environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
languages = json.loads((ROOT / 'src/deploy/localization/manifest.json').read_text())['languages']
results = []
for language in languages:
    output = args.output.resolve() / language
    command = [str(component / 'vgui_tests'), str(runtime / 'vgui2.dylib'),
               str(runtime / 'filesystem_stdio.dylib'), str(component / 'resources'),
               str(ROOT / 'src/deploy/zbot/cstrike'), str(game / 'platform'), str(game / 'valve'),
               str(output), language]
    result = subprocess.run(command, env=environment, text=True, capture_output=True)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'run.log').write_text(result.stdout + result.stderr)
    results.append(dict(language=language, command=command, exit=result.returncode))
    print(language + ': ' + ('PASS' if result.returncode == 0 else 'FAIL'))
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit'] != 0 for result in results))
