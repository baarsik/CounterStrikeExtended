#!/usr/bin/env python3
"""Analyze bounded fixed-demo series; integer host_speeds FPS is quantized, not a clock trace."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics

PROFILE = re.compile(r'(\d+) fps -- host\(\s*\d+\) sv\(\s*\d+\) cl\(\s*\d+\) '
                     r'gfx\(\s*\d+\) snd\(\s*\d+\) ents\(\d+\)')
REPORT = re.compile(r'(\d+) frames\s+([\d.]+) seconds\s+([\d.]+) fps')
DEMOS = {'cstrike': 'fb1a18edf31182d49b022a46850df0cd263fe1ea101b28caac78af038fdf211c',
         'valve': '3177530c2d13a5e88f2ffc4f86f0baefe939034cde0b5c8d787583f4c035ffa3'}


def frame_bounds_ms(fps):
    if fps < 0:
        raise ValueError('Negative profile FPS')
    if fps == 0:
        return 1000.0, math.inf
    # Host_Speeds substitutes 999 below 0.1ms; genuine floor(1/frameTime)==999 also prints 999.
    return (0.0 if fps == 999 else 1000.0 / (fps + 1)), 1000.0 / fps


def summarize_profiles(fps):
    if not fps:
        raise ValueError('No profiles')
    bounds = [frame_bounds_ms(value) for value in fps]
    lower = sorted(bound[0] for bound in bounds)
    upper = sorted(bound[1] for bound in bounds)

    def interval(index):
        low, high = lower[index], upper[index]
        return {'lower': low, 'upper': high if math.isfinite(high) else None,
                'midpoint_estimate': (low + high) / 2 if math.isfinite(high) else None}

    return {'count': len(fps), 'zero_fps_count': fps.count(0), 'fps_999_count': fps.count(999),
            'quantiles_ms': {f'p{q}': interval(math.ceil(q / 100 * len(fps)) - 1) for q in (50, 95, 99)},
            'maximum_ms': interval(len(fps) - 1)}


def parse_replay(console):
    markers = ('CSE_BENCH_START', 'Host_EndGame: End of demo.', 'CSE_BENCH_DONE')
    if any(console.count(marker) != 1 for marker in markers):
        raise ValueError('Expected one start / full EOF / final marker')
    start, end, done = (console.index(marker) for marker in markers)
    if not start < end < done:
        raise ValueError('Replay markers out of order')
    reports = list(REPORT.finditer(console))
    if len(reports) != 1 or not end < reports[0].start() < done:
        raise ValueError('Missing/ambiguous/out-of-order CLI timedemo report')
    frames, seconds, fps = reports[0].groups()
    frames, seconds, fps = int(frames), float(seconds), float(fps)
    if frames < 650 or seconds <= 0 or fps <= 0:
        raise ValueError('Partial replay or nonpositive CLI timing')
    profiles = [int(match.group(1)) for match in PROFILE.finditer(console[start:end])]
    if not frames - 1 <= len(profiles) <= frames + 2:
        raise ValueError(f'Implausible profile coverage: {len(profiles)} / {frames} CLI frames')
    return {'cli': {'frames': frames, 'seconds': seconds, 'fps': fps, 'mean_frame_ms': 1000 / fps},
            'profiles': summarize_profiles(profiles)}


def paired_summary(pairs):
    if not pairs or any(a <= 0 or b <= 0 for a, b in pairs):
        raise ValueError('Need positive paired CLI FPS')
    changes = [(b / a - 1) * 100 for a, b in pairs]
    return {'pair_count': len(pairs), 'baseline_median_cli_fps': statistics.median(a for a, _ in pairs),
            'candidate_median_cli_fps': statistics.median(b for _, b in pairs),
            'paired_fps_change_percent': changes, 'median_paired_fps_change_percent': statistics.median(changes),
            'paired_mean_frame_change_percent': [(a / b - 1) * 100 for a, b in pairs],
            'median_paired_mean_frame_change_percent': statistics.median((a / b - 1) * 100 for a, b in pairs)}


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def analyze_series(path):
    series = read_json(path)
    if series.get('success') is not True:
        raise ValueError(f'Unsuccessful series: {path}')
    game = series['game']
    if series['demo_sha256'].lower() != DEMOS.get(game):
        raise ValueError('Series does not use the accepted fixed recording')
    settings_path = path.with_name(path.name.removesuffix('-series.json') + '-settings.cfg')
    if hashlib.sha256(settings_path.read_bytes()).hexdigest() != series['settings_sha256'].lower():
        raise ValueError('Archived shared settings hash mismatch')
    count = series['pairs']
    if not isinstance(count, int) or not 1 <= count <= 30:
        raise ValueError('Invalid pair count')
    runs = {}
    ordered = []
    for filename in series['runs']:
        if Path(filename).name != filename or not filename.endswith('-result.json'):
            raise ValueError('Run reference must be a local result filename')
        result = read_json(path.parent / filename)
        key = (result['pair'], result['version'])
        if key in runs or result.get('success') is not True or result.get('exit_code') != 0:
            raise ValueError('Duplicate or unsuccessful run')
        if result.get('forced_cleanup') or result['game'] != game or result['demo_sha256'].lower() != DEMOS[game]:
            raise ValueError('Forced cleanup or game/demo identity mismatch')
        if result['warmup'] != (result['pair'] == 0):
            raise ValueError('Wrong warmup classification')
        if result['series'] != path.name or filename != result['prefix'] + '-result.json':
            raise ValueError('Wrong series/prefix binding')
        samples = result['window_samples']
        active = [i for i, sample in enumerate(samples) if sample['owned_foreground']]
        if not active:
            raise ValueError('No visible owned foreground game window')
        for sample in samples[active[0]:]:
            if not sample['owned_foreground'] and sample['elapsed_ms'] < result['wall_seconds'] * 1000 - 250:
                raise ValueError('Owned window hidden/background before process shutdown')
            if sample['visible'] and (sample['minimized'] or not sample['owned_foreground']):
                raise ValueError('Owned window lost foreground/minimized')
            if sample['owned_foreground'] and sample['foreground_pid'] != result['pid']:
                raise ValueError('Window foreground PID mismatch')
        logs = [path.parent / (result['prefix'] + f'-qconsole-{index}.log') for index in (0, 1)]
        console = '\n'.join(log.read_text(encoding='utf-8', errors='replace') for log in logs if log.exists())
        parsed = parse_replay(console)
        cli = parsed['cli']
        if any(result[name] != cli[name] for name in ('frames', 'seconds', 'fps')):
            raise ValueError('Result timing disagrees with raw console')
        if result['profile_count'] != parsed['profiles']['count']:
            raise ValueError('Result count disagrees with raw profiles')
        parsed.update({'pair': result['pair'], 'version': result['version'], 'warmup': result['warmup'],
                       'evidence': filename, 'wall_seconds': result['wall_seconds']})
        runs[key] = parsed
        ordered.append(key)
    expected = [(pair, version) for pair in range(count + 1)
                for version in (('baseline', 'candidate') if pair % 2 == 0 else ('candidate', 'baseline'))]
    if ordered != expected:
        raise ValueError('Incomplete/misordered AB/BA pairs or missing warmups')
    pairs = [(runs[pair, 'baseline']['cli']['fps'], runs[pair, 'candidate']['cli']['fps'])
             for pair in range(1, count + 1)]
    return {'game': game, 'series': str(path), 'demo_sha256': series['demo_sha256'],
            'instrumentation': series['instrumentation'], 'settings_sha256': series['settings_sha256'],
            'baseline_commit': series.get('baseline_commit', ''), 'candidate_commit': series.get('candidate_commit', ''),
            'summary': paired_summary(pairs),
            'runs': [runs[key] for key in expected], 'qualification': 'smoke only' if count < 7 else 'paired VM measurement'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('series', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = {'schema_version': 1, 'success': True, 'series': [analyze_series(path) for path in args.series],
                  'limits': [
                      'VM1 Windows x86 emulation; results do not predict native hardware.',
                      'host_speeds/condebug and 100 ms window observations are instrumentation overhead.',
                      'Profiles include demo load/sign-on host frames before EOF; not presentation events.',
                      'Nearest-rank profile quantiles bound integer-FPS-derived frame times; midpoint is only an estimate.',
                      'Ordinary FPS f>0: (1000/(f+1),1000/f] ms; 0: (1000,infinity); 999: [0,1000/999] ms.',
                      'JSON null upper bound means unbounded; zero/slow frames are retained.',
                      'CLI mean timings are separately rounded aggregate engine reports.',
                      'Warmups excluded from paired summaries; seven pairs are descriptive, not a significance claim.',
                      'PresentMon sparse OpenGL events excluded; no uninstrumented pacing claim.'
                  ]}
    except (ValueError, KeyError, OSError, TypeError) as error:
        parser.exit(1, f'Benchmark evidence rejected: {error}\n')
    # Never overwrite an analysis artifact from another run.
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(json.dumps([item['summary'] for item in result['series']], indent=2))


if __name__ == '__main__':
    main()
