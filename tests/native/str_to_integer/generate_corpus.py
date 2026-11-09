"""Author decimal-only byte corpus; retained conversion units are bytes, not Unicode scalars."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
cases = []
def add(name, value):
    data = value.encode('ascii') if isinstance(value, str) else value
    cases.append({'name': name, 'hex': data.hex()})
for name, value in [('empty', ''), ('zero', '0'), ('negative-zero', '-0'), ('positive-zero', '+0'),
        ('space', ' '), ('whitespace-only', ' \t\n\r\v\f'), ('plus', '+'), ('minus', '-'),
        ('space-plus', ' \t+'), ('space-minus', '\r -'), ('double-plus', '++7'), ('double-minus', '--7'),
        ('mixed-sign', '+-7'), ('sign-space', '- 7'), ('plain', '123'), ('trailing', '-123tail'),
        ('leading-space', ' \t\n+123rest'), ('hex-prefix', '0x123'), ('hex-negative', '-0Xf'),
        ('binary-prefix', '0b11'), ('octal-looking', '077'), ('decimal-dot', '12.3'), ('exponent', '1e9')]:
    add(name, value)
for magnitude in [1, 2147483646, 2147483647, 2147483648, 2147483649, 4294967294, 4294967295, 4294967296,
        4294967297, 9223372036854775807, 9223372036854775808, 18446744073709551615, 18446744073709551616]:
    for sign, prefix in [('plain', ''), ('plus', '+'), ('minus', '-')]:
        add(f'boundary-{sign}-{magnitude}', prefix + str(magnitude) + 'tail')
for byte in range(256):
    for name, tail in [('digit', b'7'), ('positive', b'+7'), ('negative', b'-7')]:
        add(f'prefix-{byte:02x}-{name}', bytes([byte]) + tail)
    add(f'suffix-{byte:02x}', b'17' + bytes([byte]) + b'9')
for name, data in [('utf8-nbsp', b'\xc2\xa0'), ('byte-nbsp', b'\xa0'), ('utf8-ideographic-space', b'\xe3\x80\x80'),
        ('utf8-arabic-digit', b'\xd9\xa7'), ('utf8-fullwidth-digit', b'\xef\xbc\x97')]:
    add(name, data + b'7')
    add(name + '-after-sign', b'-' + data + b'7')
for count in [64, 10000]:
    for sign in ['', '+', '-']:
        add(f'huge-{len(sign)}-{ord(sign) if sign else 0}-{count}', sign + '9' * count + 'tail')
        add(f'leading-zero-{len(sign)}-{ord(sign) if sign else 0}-{count}', sign + '0' * count + '17tail')
add('huge-whitespace-only', ' ' * 1024)
add('embedded-nul', b'12\0' + b'99999999999999999999999')
assert len({x['name'] for x in cases}) == len(cases)
(a.output / 'corpus.json').write_text(json.dumps(cases, indent=2) + '\n')
lines = ['#include <string>', '#include <vector>', 'struct InputCase { const char* name; std::string text; };',
         'inline std::vector<InputCase> InputCases() {', '    return {']
for c in cases:
    data = bytes.fromhex(c['hex'])
    encoded = ''.join('\\x%02x' % b for b in data)
    lines.append('        {"%s", std::string("%s", %d)},' % (c['name'], encoded, len(data)))
lines += ['    };', '}', '']
(a.output / 'input_cases.h').write_text('\n'.join(lines))
print(len(cases), 'byte cases', hashlib.sha256((a.output / 'corpus.json').read_bytes()).hexdigest())
