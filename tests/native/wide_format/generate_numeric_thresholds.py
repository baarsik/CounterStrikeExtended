"""Author named raw-bit neighbors; expected formatting comes only from actual UCRT."""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
entries = []
def add(name, bits):
    if 0 <= bits <= 0x7fefffffffffffff:
        entries.append({'id': name + '-positive', 'bits': f'{bits:016x}'})
        entries.append({'id': name + '-negative', 'bits': f'{bits | (1 << 63):016x}'})
def ratio(bits):
    exponent = (bits >> 52) & 2047
    mantissa = (bits & ((1 << 52) - 1)) | ((1 << 52) if exponent else 0)
    power = (exponent - 1023 if exponent else -1022) - 52
    return (mantissa << power, 1) if power >= 0 else (mantissa, 1 << -power)
for precision in (0, 1, 6, 17, 100, 322, 323):
    denominator = 10 ** (precision + 1)
    low = 0
    high = 0x7fefffffffffffff
    while low < high:
        middle = (low + high) // 2
        numerator, binary_denominator = ratio(middle)
        if numerator * denominator >= binary_denominator:
            high = middle
        else:
            low = middle + 1
    for neighbor in range(-2, 3):
        add(f'DEC-P{precision}-first-significant-neighbor-{neighbor:+d}', low + neighbor)
for precision in (0, 1, 2, 12):
    shift = (13 - precision) * 4
    bases = [0]
    if precision:
        bases += [1 << shift, (1 << 52) - (1 << shift)]
    for exponent in (0, 1, 1023, 2046):
        for kept in bases:
            for tail_id, tail in [('zero', 0), ('tail-only', 1), ('nibble0-last-tail', (1 << (shift - 4)) - 1),
                                  ('nibble1-first', 1 << (shift - 4)), ('nibble1-next', (1 << (shift - 4)) + 1),
                                  ('half-below', (1 << (shift - 1)) - 1), ('half', 1 << (shift - 1)),
                                  ('half-above', (1 << (shift - 1)) + 1)]:
                fraction = kept + tail
                if fraction < (1 << 52):
                    add(f'HEX-P{precision}-E{exponent}-K{kept:013x}-{tail_id}', (exponent << 52) | fraction)
for sign in (0, 1 << 63):
    for fraction in (1, 2, (1 << 51) - 1, 1 << 51, (1 << 51) + 1, (1 << 52) - 1):
        entries.append({'id': f'NAN-S{sign >> 63}-fraction-{fraction:013x}',
                        'bits': f'{sign | (2047 << 52) | fraction:016x}'})
unique = list(dict.fromkeys(x['bits'] for x in entries))
header = ['#include <cstdint>', '#include <vector>', 'inline std::vector<std::uint64_t> ThresholdInputs() {',
          '    return {'] + ['        0x' + bits + 'ULL,' for bits in unique] + ['    };', '}', '']
(a.output / 'numeric_thresholds.h').write_text('\n'.join(header))
(a.output / 'thresholds.json').write_text(json.dumps({'raw_regression_ids': entries, 'unique_raw_bits': len(unique)},
                                                   indent=2) + '\n')
print('named threshold entries', len(entries), 'unique bits', len(unique))
