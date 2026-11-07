#!/usr/bin/env python3
"""Check captured music phases; game sound effects must be muted by invoke_music_probe.ps1."""
import argparse
from array import array
import json
import math
from pathlib import Path
import re
import struct
import sys


def read_wave(path):
    data = path.read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('Not a RIFF/WAVE capture')
    position = 12
    fmt = None
    pcm = None
    while position + 8 <= len(data):
        name = data[position:position + 4]
        length, = struct.unpack_from('<I', data, position + 4)
        chunk = data[position + 8:position + 8 + length]
        if len(chunk) != length:
            raise ValueError('Truncated wave chunk')
        if name == b'fmt ':
            fmt = chunk
        if name == b'data':
            pcm = chunk
        position += 8 + length + length % 2
    if fmt is None or pcm is None or len(fmt) < 16:
        raise ValueError('Missing wave format/data')
    encoding, channels, rate, _, alignment, bits = struct.unpack_from('<HHIIHH', fmt)
    if encoding == 0xfffe and len(fmt) >= 40:
        encoding, = struct.unpack_from('<H', fmt, 24)
    if encoding == 3 and bits == 32:
        samples = array('f')
        scale = 1.0
    elif encoding == 1 and bits == 16:
        samples = array('h')
        scale = 32768.0
    else:
        raise ValueError(f'Unsupported captured format: encoding={encoding}, bits={bits}')
    if channels < 1 or rate < 1 or alignment != channels * bits // 8 or len(pcm) % alignment:
        raise ValueError('Invalid wave frame alignment')
    samples.frombytes(pcm)
    if sys.byteorder != 'little':
        samples.byteswap()
    return samples, scale, channels, rate


def analyze(prefix):
    result = json.loads(Path(str(prefix) + '-result.json').read_text(encoding='utf-8-sig'))
    if not result.get('success'):
        raise ValueError('Music probe did not complete')
    phases = result['phases']
    order = ['FULL', 'PAUSE', 'RESUME', 'LOW', 'FADE', 'FADED', 'STOCK', 'REPLACED', 'STOP', 'DONE']
    samples, scale, channels, rate = read_wave(Path(str(prefix) + '.wav'))
    spans = []
    for line in Path(str(prefix) + '-audio-spans.csv').read_text().splitlines():
        offset, timestamp, count = line.split(',')
        spans.append((int(offset) // (samples.itemsize * channels), float(timestamp),
                      int(count) // (samples.itemsize * channels)))
    rms = {}
    coverage = {}
    for phase, next_phase in zip(order, order[1:]):
        if phase == 'FADE':
            continue
        start = phases[phase] + 0.4
        end = phases[next_phase] - 0.3
        if end - start < 0.1:
            raise ValueError('Insufficient captured interval for ' + phase)
        square_sum = 0.0
        sample_count = 0
        for offset, timestamp, frames in spans:
            first = max(0, int((start - timestamp) * rate))
            last = min(frames, int((end - timestamp) * rate))
            if last <= first:
                continue
            segment = samples[(offset + first) * channels:(offset + last) * channels]
            square_sum += sum((value / scale) ** 2 for value in segment)
            sample_count += len(segment)
        coverage[phase] = sample_count / (rate * channels)
        # WASAPI emits no packets when the endpoint is idle. Timestamp every
        # received buffer instead of treating a silence-stripped WAV as wall time.
        if phase not in ['PAUSE', 'FADED', 'STOP'] and coverage[phase] < (end - start) * 0.6:
            raise ValueError('Audio capture timeline does not cover ' + phase)
        rms[phase] = math.sqrt(square_sum / sample_count) if sample_count else 0.0
    full = rms['FULL']
    if phases['PAUSE'] - phases['FULL'] < 2.0:
        raise ValueError('One-second track did not run long enough to demonstrate looping')
    if full < 0.0001:
        raise ValueError('No measurable music output')
    for phase in ['PAUSE', 'FADED', 'STOP']:
        if rms[phase] > full * 0.02 + 0.000001:
            raise ValueError(phase + ' did not silence music')
    if not 0.65 <= rms['RESUME'] / full <= 1.35:
        raise ValueError('Resumed music amplitude differs from full volume')
    if not 0.15 <= rms['LOW'] / full <= 0.35:
        raise ValueError('Quarter-volume output is outside the expected range')
    for phase in ['STOCK', 'REPLACED']:
        if rms[phase] < 0.00001:
            raise ValueError(phase + ' produced no measurable music')
    console = Path(str(prefix) + '-console.log').read_text(encoding='utf-8-sig')
    if 'Unable to open MP3 music' in console:
        raise ValueError('An expected music file failed to open')
    if not re.search(r'Current MP3 Volume:\s*0\.2500', console):
        raise ValueError('Music info did not report the requested quarter volume')
    return {'success': True, 'rms': rms, 'coverage_seconds': coverage, 'quarter_volume_ratio': rms['LOW'] / full,
            'loop_seconds': phases['PAUSE'] - phases['FULL'], 'decoder_sha256': result['decoder_sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path, help='Evidence prefix without -result.json or .wav')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        report = analyze(args.prefix)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, KeyError) as error:
        print('Music capture validation failed: ' + str(error), file=sys.stderr)
        return 1
    print('Music output phases passed: ' + str(args.output))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
